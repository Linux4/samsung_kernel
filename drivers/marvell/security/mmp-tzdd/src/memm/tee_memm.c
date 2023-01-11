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
 */
#ifndef CONFIG_MINI_TZDD
#include "tee_memm_internal.h"
#else
#include "mt_memm_internal.h"
#define MMU_ENABLE_BIT	(1)
#endif

#define MAGIC_NUM   (4)

#define INIT_MEMM_MAGIC(_n)  \
	do {                            \
		((uint8_t *)_n)[0] = 'M';   \
		((uint8_t *)_n)[1] = 'E';   \
		((uint8_t *)_n)[2] = 'M';   \
		((uint8_t *)_n)[3] = 'M';   \
	} while (0);

#define IS_MEMM_MAGIC_VALID(_n)  \
	(('M' == ((uint8_t *)_n)[0]) && \
	('E' == ((uint8_t *)_n)[1]) && \
	('M' == ((uint8_t *)_n)[2]) && \
	('M' == ((uint8_t *)_n)[3]))

#define CLEANUP_MEMM_MAGIC(_n) \
	do {                            \
		((uint8_t *)_n)[0] = '\0';   \
		((uint8_t *)_n)[1] = '\0';   \
		((uint8_t *)_n)[2] = '\0';   \
		((uint8_t *)_n)[3] = '\0';   \
	} while (0);

#define TEE_MEMM_UNINIT	(0xACACACAC)

typedef struct _tee_memm_handle_t {
	uint8_t magic[MAGIC_NUM];
	uint32_t reserve;
} tee_memm_handle_t;

#ifdef CONFIG_MINI_TZDD

#ifdef CONFIG_64BIT
/* The value of __PAGE_SIZE should be aligned with the one in OBM
 * Loader/Platforms/EDEN/mmu.c
 */
#define __PAGE_SIZE		(0x200000)
typedef union {
	struct {
		ulong_t  rsvd:56;
		uint32_t inner:4;
		uint32_t outer:4;
	} bits;
	ulong_t all;
} phy_addr_reg;
#else
typedef union {
	struct {
		ulong_t  rsvd:2;
		uint32_t outer:2;
		uint32_t inner:3;
	} bits;
	uint32_t all;
} phy_addr_reg;
#endif

#define PHYS_ADDR_BITS (0xFFFFFFFFFFFF)
static void *_osa_virt_to_phys_by_cp15_ex(ulong_t virt, bool *cacheable)
{
#ifdef CONFIG_64BIT
	volatile ulong_t ret;
	phy_addr_reg par;

	__asm__ __volatile__("at s1e2r, %0" : : "r"(virt));
	__asm__ __volatile__("mrs %0, par_el1" : "=r"(ret) : );

	par.all = ret;
	if (par.bits.outer == 0x4 &&
		par.bits.inner == 0x4)
		*cacheable = false;
	else
		*cacheable = true;

	ret &= ~(__PAGE_SIZE - 1);
	ret |= (virt & (__PAGE_SIZE - 1));
	ret &= PHYS_ADDR_BITS;

	return (void *)ret;
#else
	volatile ulong_t ret;
	phy_addr_reg par;

	/* fail to get the phys_addr when using 0, c7, c8, 1 */
	__asm__ __volatile__("mcr p15, 0, %1, c7, c8, 0\n\t"
		"mrc p15, 0, %0, c7, c4, 0\n\t" : "=r"(ret) : "r"
		(virt));

	par.all = ret;
	if (par.bits.outer == 0x0 &&
	   (par.bits.inner & 0x4) == 0x0)
		*cacheable = false;
	else if (par.bits.outer > 0x0 &&
			(par.bits.inner & 0x4) == 0x4)
		*cacheable = true;
	else
		OSA_ASSERT(0);
	ret &= ~(PAGE_SIZE - 1);
	ret |= (virt & (PAGE_SIZE - 1));

	return (void *)ret;
#endif
}

#endif /* CONFIG_MINI_TZDD */

static inline void *osa_ext_virt_to_phys(void *virt_addr, bool *cacheable_cur)
{
#ifdef CONFIG_MINI_TZDD
	volatile ulong_t ret;

#ifdef CONFIG_64BIT
	__asm__ __volatile__("mrs %0, SCTLR_EL2" : "=r"(ret) : );
#else
	__asm__ __volatile__("MRC p15, 0, %0, c1, c0, 0" : "=r"(ret) : );
#endif

	if (ret & MMU_ENABLE_BIT) {
		/* MMU enabled in EL2 */
		void *phys_addr;
		phys_addr = _osa_virt_to_phys_by_cp15_ex(
			(ulong_t)virt_addr, cacheable_cur);
		return phys_addr;
	} else {
		return virt_addr;
	}

#else
	return osa_virt_to_phys_ex((void *)virt_addr, cacheable_cur);
#endif
}

tee_memm_ss_t tee_memm_create_ss(void)
{
	tee_memm_handle_t *tee_memm_handle;

	tee_memm_handle = osa_kmem_alloc(sizeof(tee_memm_handle_t));
	if (!tee_memm_handle)
		return NULL;

	memset(tee_memm_handle, 0, sizeof(tee_memm_handle_t));
	INIT_MEMM_MAGIC(tee_memm_handle->magic);

	return (tee_memm_ss_t) tee_memm_handle;
}

void tee_memm_destroy_ss(tee_memm_ss_t tee_memm_ss)
{
	tee_memm_handle_t *tee_memm_handle;

	if (!tee_memm_ss)
		return;

	tee_memm_handle = (tee_memm_handle_t *) tee_memm_ss;
	if (!(IS_MEMM_MAGIC_VALID(tee_memm_handle->magic)))
		return;

	CLEANUP_MEMM_MAGIC(tee_memm_handle->magic);

	osa_kmem_free(tee_memm_handle);

	return;
}

/* if pages = NULL, return pages_num
 * if pages_num = NULL, return pages
*/
tee_stat_t tee_memm_set_phys_pages(tee_memm_ss_t tee_memm_ss,
				void *virt, uint32_t size,
				tee_mem_page_t *pages, uint32_t *pages_num)
{
	uint32_t num = 0;
	ulong_t base = (ulong_t)virt;
	uint32_t length = size;
	tee_memm_handle_t *tee_memm_handle;

	uint8_t *buf;
	tee_mem_page_t *page;
	tee_mem_page_t tmp;
	uint32_t tmp_len = 0;
	uint32_t count = 0;
	bool cacheable_cur = false, cacheable_base = false;

	if (!tee_memm_ss || !virt)
		OSA_ASSERT(0);

	if (!pages && !pages_num)
		OSA_ASSERT(0);

	tee_memm_handle = (tee_memm_handle_t *) tee_memm_ss;
	if (!(IS_MEMM_MAGIC_VALID(tee_memm_handle->magic)))
		OSA_ASSERT(0);

	size += base & (~PAGE_MASK);
	num = size / PAGE_SIZE;
	if (size & (PAGE_SIZE - 1))
		num ++;

	/* NOTE: num >= count, so alloc enough memory. !!!FIXME!!! */
	buf = osa_vmem_alloc(
            num * sizeof(tee_mem_page_t), OSA_MEM_READ_WRITE);
	OSA_ASSERT(buf != NULL);

	memset(buf, 0, sizeof(num * sizeof(tee_mem_page_t)));

	page = (tee_mem_page_t *)buf;
	tmp.phy_addr = NULL;
	tmp.len = 0;

	page->phy_addr = osa_ext_virt_to_phys((void *)base, &cacheable_base);
	while (base < ((ulong_t)virt + length)) {
		page->phy_addr = osa_ext_virt_to_phys((void *)base, &cacheable_cur);
		if (base == (ulong_t) virt) {
			/* the first page */
			tmp.len = PAGE_SIZE - (base - (base & PAGE_MASK));
			if (tmp.len > length)
				tmp.len = length;
			tmp_len = tmp.len;
		} else if (((ulong_t) virt + length) - base <= PAGE_SIZE) {
			/* the last page */
			tmp_len = ((ulong_t) virt + length) - base;
		} else {
			tmp_len = PAGE_SIZE;
		}

		if (page->phy_addr == (tmp.phy_addr + tmp.len) &&
			cacheable_cur == cacheable_base) {
			page--;
			page->len += tmp_len;
		} else {
			page->len = tmp_len;
			page->cacheable = cacheable_cur;
			count++;
		}

		tmp.phy_addr = page->phy_addr;
		tmp.len = page->len;
		page++;

		base = (base & PAGE_MASK) + PAGE_SIZE;
		cacheable_base = cacheable_cur;
	}

	if (pages_num)
		*pages_num = count;

	if (pages)
		memcpy(pages, buf, count * sizeof(tee_mem_page_t));
	osa_vmem_free(buf);

	return TEEC_SUCCESS;
}

#ifndef CONFIG_MINI_TZDD
tee_stat_t tee_memm_get_user_mem(tee_memm_ss_t tee_memm_ss,
				void __user *virt, uint32_t size,
				void __kernel **kvirt)
{
	ulong_t start = (ulong_t) virt;
	ulong_t end = (start + size + PAGE_SIZE - 1) & PAGE_MASK;

	struct vm_area_struct *vma;
	ulong_t cur = start & PAGE_MASK, cur_end;
	int32_t err, write;
	tee_memm_handle_t *tee_memm_handle;

	OSA_ASSERT(virt && tee_memm_ss);

	tee_memm_handle = (tee_memm_handle_t *) tee_memm_ss;
	if (!(IS_MEMM_MAGIC_VALID(tee_memm_handle->magic)))
		OSA_ASSERT(0);

	down_read(&current->mm->mmap_sem);

	do {
		vma = find_vma(current->mm, cur);
		if (!vma) {
			up_read(&current->mm->mmap_sem);
			*kvirt = NULL;
			return TEEC_SUCCESS;
		}
		cur_end = ((end <= vma->vm_end) ? end : vma->vm_end);

		/* this vma is NOT mmap-ed and pfn-direct mapped */
		if (!(vma->vm_flags & (VM_IO | VM_PFNMAP))) {
			write = ((vma->vm_flags & VM_WRITE) != 0);
			/* NOTE: no get_page since overall sync-flow */
			err = get_user_pages(current, current->mm,
						(ulong_t) cur,
						(cur_end - cur) >> PAGE_SHIFT,
						write, 0, NULL, NULL);
			if (err < 0) {
				up_read(&current->mm->mmap_sem);
				*kvirt = NULL;
				return TEEC_SUCCESS;
			}
		}

		cur = cur_end;
	} while (cur_end < end);

	up_read(&current->mm->mmap_sem);

	*kvirt = virt;

	/* fake wtm_vm handle here */
	return TEEC_SUCCESS;
}

void tee_memm_put_user_mem(tee_memm_ss_t tee_memm_ss, void *vm)
{
	tee_memm_handle_t *tee_memm_handle;

	tee_memm_handle = (tee_memm_handle_t *) tee_memm_ss;
	if (!(IS_MEMM_MAGIC_VALID(tee_memm_handle->magic)))
		OSA_ASSERT(0);

	OSA_ASSERT(vm);

	/* nothing */
	return;
}
#endif /* CONFIG_MINI_TZDD */
