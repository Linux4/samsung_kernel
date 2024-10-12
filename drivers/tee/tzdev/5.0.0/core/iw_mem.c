/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "core/iw_mem.h"
#include "core/iw_mem_impl.h"
#include "core/log.h"
#include "core/smc_channel.h"
#include "core/sysdep.h"

enum {
	PAGES_TYPE_KERNEL,
	PAGES_TYPE_EXIST,
	PAGES_TYPE_USER,
};

struct tzdev_iw_mem {
	unsigned int pages_type;
	size_t pages_count;
	struct page **pages;
	void *map_address;
	struct mm_struct *mm;
	void *impl_data;
};

static const struct tzdev_iw_mem_impl *impl;

/**
 * tzdev_iw_mem_create() - Create iw memory.
 * @size: size
 *
 * Allocates pages enough to store @size bytes and creates iw memory over them.
 *
 * Return: pointer to iw memory and error on failure.
 */
struct tzdev_iw_mem *tzdev_iw_mem_create(size_t size)
{
	struct tzdev_iw_mem *mem;
	struct page **pages;
	size_t pages_count = PAGE_ALIGN(size) / PAGE_SIZE;
	unsigned int i;
	int ret;

	if (!pages_count)
		return ERR_PTR(-EINVAL);

	mem = kzalloc(sizeof(struct tzdev_iw_mem), GFP_KERNEL);
	if (!mem)
		return ERR_PTR(-ENOMEM);

	pages = kmalloc_array(pages_count, sizeof(struct pages *), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		goto free_mem;
	}

	for (i = 0; i < pages_count; i++) {
		pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!pages[i]) {
			ret = -ENOMEM;
			goto free_pages;
		}
	}

	if (impl->data_size) {
		mem->impl_data = kzalloc(impl->data_size, GFP_KERNEL);
		if (!mem->impl_data) {
			ret = -ENOMEM;
			goto free_pages;
		}
	}

	ret = impl->init(pages, pages_count, mem->impl_data, true);
	if (ret)
		goto free_impl_data;

	mem->pages_type = PAGES_TYPE_KERNEL;
	mem->pages_count = pages_count;
	mem->pages = pages;

	return mem;

free_impl_data:
	kfree(mem->impl_data);
free_pages:
	if (ret != -EADDRNOTAVAIL) {
		for (i = 0; i < pages_count; i++) {
			if (!pages[i])
				break;
			__free_page(pages[i]);
		}
	}
	kfree(pages);
free_mem:
	kfree(mem);
	return ERR_PTR(ret);
}

/**
 * tzdev_iw_mem_create_exist() - Create iw memory over existing memory.
 * @ptr: address
 * @size: size
 *
 * Gets pages that cover [@ptr; @ptr+@size) kernel virtual memory and creates iw memory over them.
 *
 * Return: pointer to iw memory and error on failure.
 */
struct tzdev_iw_mem *tzdev_iw_mem_create_exist(void *ptr, size_t size)
{
	struct tzdev_iw_mem *mem;
	struct page **pages;
	size_t pages_count = PAGE_ALIGN(size) / PAGE_SIZE;
	unsigned long addr = (unsigned long)ptr;
	unsigned int i;
	int ret;

	if (!pages_count)
		return ERR_PTR(-EINVAL);

	mem = kzalloc(sizeof(struct tzdev_iw_mem), GFP_KERNEL);
	if (!mem)
		return ERR_PTR(-ENOMEM);

	pages = kmalloc_array(pages_count, sizeof(struct pages *), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		goto free_mem;
	}

	for (i = 0; i < pages_count; i++) {
		pages[i] = is_vmalloc_addr((void *)(addr + PAGE_SIZE * i))
				? vmalloc_to_page((void *)(addr + PAGE_SIZE * i))
				: virt_to_page(addr + PAGE_SIZE * i);
		if (!pages[i]) {
			ret = -ENOMEM;
			goto free_pages;
		}
	}

	if (impl->data_size) {
		mem->impl_data = kzalloc(impl->data_size, GFP_KERNEL);
		if (!mem->impl_data) {
			ret = -ENOMEM;
			goto free_pages;
		}
	}

	ret = impl->init(pages, pages_count, mem->impl_data, false);
	if (ret)
		goto free_impl_data;

	mem->pages_type = PAGES_TYPE_EXIST;
	mem->pages_count = pages_count;
	mem->pages = pages;

	return mem;

free_impl_data:
	kfree(mem->impl_data);
free_pages:
	kfree(pages);
free_mem:
	kfree(mem);
	return ERR_PTR(ret);
}

/**
 * tzdev_iw_mem_destroy() - Destroy iw memory.
 * @mem: iw memory
 *
 * Unmaps mapping that is created via tzdev_iw_mem_map(), releases pages according to their type
 * (kernel, exist, user) and frees @mem structure.
 *
 * Before calling this function be sure that there are no mappings created via tzdev_iw_mem_map_user()
 * and @mem is released by SWd.
 */
void tzdev_iw_mem_destroy(struct tzdev_iw_mem *mem)
{
	int ret;
	unsigned int i;


	if (mem->map_address)
		tzdev_iw_mem_unmap(mem);

	ret = impl->deinit(mem->impl_data);
	if (mem->pages_type == PAGES_TYPE_KERNEL) {
		/* We can't use this memory anymore if EADDRNOTAVAIL*/
		if (ret != -EADDRNOTAVAIL) {
			for (i = 0; i < mem->pages_count; i++)
				__free_page(mem->pages[i]);
		}
	} else if (mem->pages_type == PAGES_TYPE_USER) {
		mmput(mem->mm);
	}

	kfree(mem->impl_data);
	kfree(mem->pages);
	kfree(mem);
}

/**
 * tzdev_iw_mem_map() - Map iw memory into kernel space.
 * @mem: iw memory
 *
 * Be careful @mem can be mapped only once.
 *
 * Return: mapped address on success and NULL on failure.
 */
void *tzdev_iw_mem_map(struct tzdev_iw_mem *mem)
{
	BUG_ON(mem->map_address);

	mem->map_address = vmap(mem->pages, mem->pages_count, VM_MAP, PAGE_KERNEL);
	return mem->map_address;
}

/**
 * tzdev_iw_mem_unmap() - Unmap iw memory from kernel space.
 * @mem: iw memory
 */
void tzdev_iw_mem_unmap(struct tzdev_iw_mem *mem)
{
	vunmap(mem->map_address);
	mem->map_address = NULL;
}

/**
 * tzdev_iw_mem_map_user() - Map iw memory into user vma.
 * @mem: iw memory
 * @vma: user vma
 *
 * Inserts @mem pages into @vma user vma.
 *
 * It should be called only from f_op->mmap() handler(under mm->mmap_sem(mm->mmap_lock) write-lock).
 *
 * Return: 0 on success and error on failure.
 */
int tzdev_iw_mem_map_user(struct tzdev_iw_mem *mem, struct vm_area_struct *vma)
{
	unsigned long pages_count = vma_pages(vma);
	unsigned long begin = vma->vm_pgoff;
	unsigned int i;
	int ret;

	if (mem->pages_count < begin + pages_count)
		return -EINVAL;

	for (i = 0; i < pages_count; i++) {
		ret = vm_insert_page(vma, vma->vm_start + i * PAGE_SIZE, mem->pages[begin + i]);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * tzdev_iw_mem_get_map_address() - Get map address.
 * @mem: iw memory
 */
void *tzdev_iw_mem_get_map_address(struct tzdev_iw_mem *mem)
{
	return mem->map_address;
}

/**
 * tzdev_iw_mem_unmap() - Get actual iw memory size.
 * @mem: iw memory
 */
size_t tzdev_iw_mem_get_size(struct tzdev_iw_mem *mem)
{
	return mem->pages_count << PAGE_SHIFT;
}

/**
 * tzdev_iw_mem_pack() - Pack iw memory into smc channel.
 * @mem: iw memory
 * @channel: smc channel
 *
 * Return: 0 on success and error on failure.
 */
int tzdev_iw_mem_pack(struct tzdev_iw_mem *mem, struct tzdev_smc_channel *channel)
{
	return impl->pack(channel, mem->pages, mem->pages_count, mem->impl_data);
}

void tz_iw_mem_init(void)
{
	impl = tzdev_get_iw_mem_impl();
}
