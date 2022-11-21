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

#include "tee_memm_internal.h"

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

typedef struct _tee_memm_handle_t {
	uint8_t magic[MAGIC_NUM];
	uint8_t *page_list_buf;
	uint32_t page_list_size;
	uint32_t reserve;
} tee_memm_handle_t;

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

	if (tee_memm_handle->page_list_buf)
		osa_kmem_free(tee_memm_handle->page_list_buf);
	osa_kmem_free(tee_memm_handle);

	return;
}

tee_stat_t tee_memm_calc_pages_num(tee_memm_ss_t tee_memm_ss,
				   void *virt, uint32_t size,
				   uint32_t *pages_num)
{
	uint32_t num = 0;
	uint32_t base = (uint32_t) virt;
	uint32_t length = size;
	tee_memm_handle_t *tee_memm_handle;

	uint8_t *buf;
	tee_mem_page_t *page;
	tee_mem_page_t tmp;
	uint32_t tmp_len = 0;
	uint32_t count = 0;

	if (!tee_memm_ss || !virt || !pages_num)
		OSA_ASSERT(0);

	tee_memm_handle = (tee_memm_handle_t *) tee_memm_ss;
	if (!(IS_MEMM_MAGIC_VALID(tee_memm_handle->magic)))
		OSA_ASSERT(0);

	size += base & (~PAGE_MASK);
	num = size / PAGE_SIZE;
	if (size & (PAGE_SIZE - 1))
		num ++;

	/* *pages_num = num; */

	buf = osa_kmem_alloc(num * sizeof(tee_mem_page_t));
	OSA_ASSERT(buf != NULL);

	page = (tee_mem_page_t *)buf;
	tmp.phy_addr = NULL;
	tmp.len = 0;
	while (base < ((ulong_t) virt + length)) {
		page->phy_addr = osa_virt_to_phys((void *)base);
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

		if (page->phy_addr == (tmp.phy_addr + tmp.len)) {
			page --;
			page->len += tmp_len;
		} else {
			page->len = tmp_len;
			count ++;
		}

		tmp.phy_addr = page->phy_addr;
		tmp.len = page->len;
		page ++;

		base = (base & PAGE_MASK) + PAGE_SIZE;
	}

	*pages_num = count;

	tee_memm_handle->page_list_buf = buf;
	tee_memm_handle->page_list_size = count * sizeof(tee_mem_page_t);

	return TEEC_SUCCESS;
}

tee_stat_t tee_memm_parse_phys_pages(tee_memm_ss_t tee_memm_ss,
				     void *virt, uint32_t length,
				     tee_mem_page_t *pages, uint32_t pages_num)
{
	tee_memm_handle_t *tee_memm_handle;

	if (!tee_memm_ss || !virt || !pages)
		OSA_ASSERT(0);

	tee_memm_handle = (tee_memm_handle_t *) tee_memm_ss;
	if (!(IS_MEMM_MAGIC_VALID(tee_memm_handle->magic)))
		OSA_ASSERT(0);

	memcpy(pages, tee_memm_handle->page_list_buf,
		tee_memm_handle->page_list_size);

	return TEEC_SUCCESS;
}

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
