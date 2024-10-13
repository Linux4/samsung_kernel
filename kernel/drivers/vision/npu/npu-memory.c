/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/device.h>

#include <media/videobuf2-dma-sg.h>
#include <linux/iommu.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/samsung/samsung-dma-mapping.h>
#include <trace/hooks/mm.h>
#include "npu-log.h"
#include "npu-binary.h"
#include "npu-memory.h"
#include "npu-session.h"
#include "npu-config.h"

static struct device npu_sync_dev;
static atomic_t total_mem_size;

/* trace npu memory for android vendor hook function */
static void npu_memory_trace_for_vh(unsigned int size, bool is_add) {
	if (is_add) atomic_add(size, &total_mem_size);
	else 		atomic_sub(size, &total_mem_size);
}

/* '$ cat /proc/meminfo' */
static void npu_meminfo_show(void *data, struct seq_file *m)
{
	unsigned long kb_size = (unsigned long)(atomic_read(&total_mem_size) >> 10);
	seq_printf(m, "NpuSystem:      %8lu kB\n", kb_size);
}

/* '$ echo m > /proc/sysrq-trigger' */
static void npu_mem_show(void *data, unsigned int filter, nodemask_t *nodemask)
{
	unsigned long kb_size = (unsigned long)(atomic_read(&total_mem_size) >> 10);
	pr_info("npu_sys_size:        %8lu kB\n", kb_size);
}

static void npu_sgt_flush(struct sg_table *sgt)
{
	struct sg_page_iter piter;
	struct page *p;

	for_each_sgtable_page(sgt, &piter, 0) {
		p = sg_page_iter_page(&piter);
		dma_sync_single_for_device(&npu_sync_dev, page_to_phys(p),  page_size(p), DMA_TO_DEVICE);
	}
}

static int npu_memory_vmap(struct npu_memory_buffer *buffer, u64 size)
{
	struct sg_table *table = buffer->sgt;
	unsigned int npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct sg_page_iter piter;

	if (!pages)
		return -ENOMEM;

	for_each_sgtable_page(table, &piter, 0) {
		WARN_ON(tmp - pages >= npages);
		*tmp++ = sg_page_iter_page(&piter);
	}

	buffer->vaddr = vmap(pages, npages, VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	vfree(pages);

	return 0;
}

static void npu_memory_free_sgt(struct sg_table *sgt)
{
	struct scatterlist *sg;

	for (sg = sgt->sgl; sg; sg = sg_next(sg))
		if (sg_page(sg))
			__free_pages(sg_page(sg), get_order(sg->length));
	sg_free_table(sgt);
	kfree(sgt);
}

static int npu_memory_create_sgt(struct npu_memory_buffer *buffer, u64 size)
{
	struct scatterlist *sg;
	struct sg_table *sgt;
	struct page **pages;
	int *pages_order;
	int buf_extra;
	int max_order;
	int nr_pages;
	int ret = 0;
	int i, j, k;
	int order;

	if (size) {
		nr_pages = DIV_ROUND_UP(size, PAGE_SIZE);
		buf_extra = (PAGE_SIZE - size % PAGE_SIZE) % PAGE_SIZE;
		max_order = min(MAX_ORDER - 1, get_order(size));
	} else {
		nr_pages = 1;
		buf_extra = 0;
		max_order = 0;
	}

	pages = kvmalloc_array(nr_pages, sizeof(*pages) + sizeof(*pages_order), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		goto out;
	}
	pages_order = (void *)pages + sizeof(*pages) * nr_pages;

	i = 0;
	while (nr_pages > 0) {
		order = min(get_order(nr_pages * PAGE_SIZE), max_order);
		while (1) {
			pages[i] = alloc_pages(GFP_HIGHUSER | __GFP_COMP |
					       __GFP_NOWARN | __GFP_ZERO |
					       (order ? __GFP_NORETRY : __GFP_RETRY_MAYFAIL),
					       order);
			if (pages[i])
				break;
			if (!order--) {
				ret = -ENOMEM;
				goto free_partial_alloc;
			}
		}

		max_order = order;
		pages_order[i] = order;

		nr_pages -= 1 << order;
		if (nr_pages <= 0)
			buf_extra += abs(nr_pages) * PAGE_SIZE;
		i++;
	}

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		ret = -ENOMEM;
		goto free_partial_alloc;
	}

	if (sg_alloc_table(sgt, i, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto free_sgt;
	}

	sg = sgt->sgl;
	for (k = 0; k < i; k++, sg = sg_next(sg)) {
		if (k < i - 1) {
			sg_set_page(sg, pages[k], PAGE_SIZE << pages_order[k], 0);
		} else {
			sg_set_page(sg, pages[k], (PAGE_SIZE << pages_order[k]) - buf_extra, 0);
			sg_mark_end(sg);
		}
	}

	buffer->sgt = sgt;

	kvfree(pages);
	return ret;

free_sgt:
	kfree(sgt);
free_partial_alloc:
	for (j = 0; j < i; j++)
		__free_pages(pages[j], pages_order[j]);
	kvfree(pages);
out:
	return ret;
}

struct dma_buf *npu_memory_ion_alloc(size_t size, unsigned int flag)
{
	struct dma_buf *dma_buf = NULL;
	struct dma_heap *dma_heap;

	dma_heap = dma_heap_find("system-uncached");
	if (dma_heap) {
		dma_buf = dma_heap_buffer_alloc(dma_heap, size, 0, flag);
		dma_heap_put(dma_heap);
	} else {
		npu_info("dma_heap is not exist\n");
	}

	return dma_buf;
}

struct dma_buf *npu_memory_ion_alloc_cached(size_t size, unsigned int flag)
{
	struct dma_buf *dma_buf = NULL;
	struct dma_heap *dma_heap;

	dma_heap = dma_heap_find("system");
	if (dma_heap) {
		dma_buf = dma_heap_buffer_alloc(dma_heap, size, 0, flag);
		dma_heap_put(dma_heap);
	}

	return dma_buf;
}

#ifndef CONFIG_NPU_KUNIT_TEST
static struct dma_buf *npu_memory_ion_alloc_secure(size_t size, unsigned int flag)
{
	struct dma_buf *dma_buf = NULL;
	struct dma_heap *dma_heap_secure;

	dma_heap_secure = dma_heap_find("secure_dsp");
	if (dma_heap_secure) {
		dma_buf = dma_heap_buffer_alloc(dma_heap_secure, size, 0, flag);
		dma_heap_put(dma_heap_secure);
	} else {
		npu_info("dma_heap_secure is not exist\n");
	}

	return dma_buf;
}
#endif

dma_addr_t npu_memory_dma_buf_dva_map(struct npu_memory_buffer *buffer)
{
	return sg_dma_address(buffer->sgt->sgl);
}

void npu_memory_dma_buf_dva_unmap(
		__attribute__((unused))struct npu_memory_buffer *buffer, bool mapping)
{
	struct iommu_domain *domain = NULL;
	size_t unmapped = 0;

	if (!mapping)
		return;

	domain = iommu_get_domain_for_dev(buffer->attachment->dev);
	if (!domain) {
		probe_warn("iommu_unmap failed\n");
		return;
	}

	unmapped = iommu_unmap(domain, buffer->daddr, buffer->size);
	if (!unmapped)
		probe_warn("iommu_unmap failed\n");
	if (unmapped != buffer->size)
		probe_warn("iomuu_unmap unmapped size(%lu) is not buffer size(%lu)\n",
				unmapped, buffer->size);
}

static void npu_dma_set_mask(struct device *dev)
{
	dma_set_mask(dev, DMA_BIT_MASK(32));
}

unsigned long npu_memory_set_prot(int prot,
		struct dma_buf_attachment *attachment)
{
	probe_info("prot : %d", prot);
	attachment->dma_map_attrs |= DMA_ATTR_SET_PRIV_DATA(prot);
	return ((prot) << IOMMU_PRIV_SHIFT);
}

void npu_dma_buf_add_attr(struct dma_buf_attachment *attachment)
{
	attachment->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
}

int npu_memory_probe(struct npu_memory *memory, struct device *dev)
{
	BUG_ON(!memory);
	BUG_ON(!dev);

	memory->dev = dev;

	npu_dma_set_mask(dev);
	device_initialize(&npu_sync_dev);
	dma_set_mask(&npu_sync_dev, DMA_BIT_MASK(32));


	spin_lock_init(&memory->map_lock);
	INIT_LIST_HEAD(&memory->map_list);
	memory->map_count = 0;

	spin_lock_init(&memory->alloc_lock);
	INIT_LIST_HEAD(&memory->alloc_list);
	memory->alloc_count = 0;

	spin_lock_init(&memory->valloc_lock);
	INIT_LIST_HEAD(&memory->valloc_list);
	memory->valloc_count = 0;

	spin_lock_init(&memory->info_lock);
	INIT_LIST_HEAD(&memory->info_list);
	memory->info_count = 0;

	atomic_set(&total_mem_size, 0);
	register_trace_android_vh_meminfo_proc_show(npu_meminfo_show, dev);
	register_trace_android_vh_show_mem(npu_mem_show, dev);

	return 0;
}

void npu_memory_release(struct npu_memory *memory)
{
	unsigned long flags;
	struct npu_memory_debug_info *info, *temp;

	if (!memory) {
		npu_info("npu_memory is NULL, skip this func.\n");
		return;
	}

	spin_lock_irqsave(&memory->info_lock, flags);
	list_for_each_entry_safe(info, temp, &memory->info_list, list) {
		list_del(&info->list);
		kfree(info);
	}
	spin_unlock_irqrestore(&memory->info_lock, flags);
}

int npu_memory_open(struct npu_memory *memory)
{
	int ret = 0;

	/* to do init code */

	return ret;
}

int npu_memory_close(struct npu_memory *memory)
{
	int ret = 0;

	/* to do cleanup code */

	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
void npu_memory_buffer_dump(struct npu_memory_buffer *buffer)
{
	if (!buffer)
		return;

	npu_dump("Memory name = %s\n", buffer->name);
	npu_dump("Memory vaddr = 0x%llx\n", (unsigned long long)buffer->vaddr);
	npu_dump("Memory daddr = 0x%llx\n", buffer->daddr);
	npu_dump("Memory size = %lu\n", buffer->size);
}

void npu_memory_buffer_health_dump(struct npu_memory_buffer *buffer)
{
	if (!buffer)
		return;

	npu_health("Memory name = %s\n", buffer->name);
	npu_health("Memory vaddr = 0x%llx\n", (unsigned long long)buffer->vaddr);
	npu_health("Memory daddr = 0x%llx\n", buffer->daddr);
	npu_health("Memory size = %lu\n", buffer->size);
}

void npu_memory_health(struct npu_memory *memory)
{
	unsigned long flags;
	struct npu_memory_buffer *npu_memory;
	struct npu_memory_debug_info *dbg_info;
	size_t	alloc_size = 0;
	size_t	map_size = 0;

	if (!memory)
		return;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	npu_health("---------- NPU Memory alloc list ----------\n");
	list_for_each_entry(npu_memory, &memory->alloc_list, list) {
		alloc_size += npu_memory->size;
	}
	npu_health("npu heap memory(alloc) total = %lu\n", alloc_size);
	npu_health("------------------------------------------------\n");
	spin_unlock_irqrestore(&memory->alloc_lock, flags);

	spin_lock_irqsave(&memory->map_lock, flags);
	npu_health("---------- NPU Memory map list ----------\n");
	list_for_each_entry(npu_memory, &memory->map_list, list) {
		map_size += npu_memory->size;
	}
	npu_health("npu heap memory(map) total = %lu, map_cnt = %u\n", map_size, memory->map_count);
	npu_health("------------------------------------------------\n");
	spin_unlock_irqrestore(&memory->map_lock, flags);

	spin_lock_irqsave(&memory->info_lock, flags);
	npu_health("---------- NPU Memory dbg info list ----------\n");
	npu_health("memory_info_count : %u\n", memory->info_count);
	list_for_each_entry(dbg_info, &memory->info_list, list) {
		npu_health("PID[%d] CUR[%s] SIZE[%lu] REF_CNT[%lld]\n",
			dbg_info->pid, dbg_info->comm, dbg_info->size,
			atomic64_read(&dbg_info->ref_count));
	}
	npu_health("------------------------------------------------\n");
	spin_unlock_irqrestore(&memory->info_lock, flags);

}

void npu_memory_dump(struct npu_memory *memory)
{
	unsigned long flags;
	struct npu_memory_buffer *npu_memory;
	struct npu_memory_v_buf *npu_vmemory;

	if (!memory)
		return;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	npu_dump("---------- NPU Memory alloc list ----------\n");
	list_for_each_entry(npu_memory, &memory->alloc_list, list) {
		npu_memory_buffer_dump(npu_memory);
		npu_dump("------------------------------------------------\n");
	}
	spin_unlock_irqrestore(&memory->alloc_lock, flags);

	spin_lock_irqsave(&memory->valloc_lock, flags);
	npu_dump("---------- NPU Memory valloc list ----------\n");
	list_for_each_entry(npu_vmemory, &memory->valloc_list, list) {
		npu_dump("Memory name = %s\n", npu_vmemory->name);
		npu_dump("Memory size = %lu\n", npu_vmemory->size);
		npu_dump("------------------------------------------------\n");
	}
	spin_unlock_irqrestore(&memory->valloc_lock, flags);

	spin_lock_irqsave(&memory->map_lock, flags);
	npu_dump("---------- NPU Memory map list ----------\n");
	list_for_each_entry(npu_memory, &memory->map_list, list) {
		npu_memory_buffer_dump(npu_memory);
		if (npu_memory->ncp)
			printk("Memory name = %s\n", npu_memory->name);
		npu_dump("------------------------------------------------\n");
	}
	spin_unlock_irqrestore(&memory->map_lock, flags);
}
#endif

int npu_memory_map(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap)
{
	int ret = 0;
	bool complete_suc = false;

	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t daddr;
	void *vaddr;

	unsigned long flags;

	BUG_ON(!memory);
	BUG_ON(!buffer);

	npu_info("Try mapping DMABUF fd=%d size=%zu\n", buffer->m.fd, buffer->size);
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
	INIT_LIST_HEAD(&buffer->list);

	dma_buf = dma_buf_get(buffer->m.fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is fail(%pK)\n", dma_buf);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma_buf = dma_buf;

	if (buffer->dma_buf->size < buffer->size) {
		npu_err("Allocate buffer size(%zu) is smaller than expectation(%u)\n",
			buffer->dma_buf->size, (unsigned int)buffer->size);
		ret = -EINVAL;
		goto p_err;
	}

	attachment = dma_buf_attach(buffer->dma_buf, memory->dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		goto p_err;
	}
	buffer->attachment = attachment;

	if (prot)
		npu_dma_buf_add_attr(attachment);

	if (lazy_unmap == NPU_SESSION_LAZY_UNMAP_DISABLE)
		buffer->attachment->dma_map_attrs |= (DMA_ATTR_SKIP_LAZY_UNMAP);

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		npu_err("dma_buf_map_attach is fail(%d) size(%zu)\n",
			ret, buffer->size);
		goto p_err;
	}
	buffer->sgt = sgt;

	daddr = npu_memory_dma_buf_dva_map(buffer);
	if (IS_ERR_VALUE(daddr)) {
		npu_err("Fail(err %pad) to allocate iova\n", &daddr);
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->daddr = daddr;

	vaddr = npu_dma_buf_vmap(buffer->dma_buf);
	if (IS_ERR(vaddr)) {
		npu_err("Failed to get vaddr (err %pK)\n", vaddr);
		ret = -EFAULT;
		goto p_err;
	}
	buffer->vaddr = vaddr;

	npu_dbg("buffer[%pK], paddr[%llx], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
		buffer, buffer->paddr, buffer->vaddr, buffer->daddr, buffer->sgt, buffer->attachment, buffer->size);

	spin_lock_irqsave(&memory->map_lock, flags);
	list_add_tail(&buffer->list, &memory->map_list);
	memory->map_count++;
	spin_unlock_irqrestore(&memory->map_lock, flags);

	complete_suc = true;
	npu_memory_alloc_dbg_info(memory, buffer->dma_buf, buffer->size);
p_err:
	if (complete_suc != true) {
		npu_memory_unmap(memory, buffer);
		npu_memory_dump(memory);
	}
	return ret;
}

void npu_memory_alloc_dbg_info(struct npu_memory *memory, struct dma_buf *dmabuf, size_t size)
{
	bool skip = false;
	unsigned long flags;
	struct npu_memory_debug_info *info, *dbg_info, *temp;

	if (!memory) {
		npu_info("npu_memory is NULL, skip this func.\n");
		return;
	}

	spin_lock_irqsave(&memory->info_lock, flags);
	list_for_each_entry_safe(info, temp, &memory->info_list, list) {
		if (info->dma_buf != (u64)dmabuf)
			continue;

		atomic64_set(&info->ref_count, atomic64_read(&dmabuf->file->f_count));
		skip = true;
		break;
	}

	if (skip)
		goto done;

	dbg_info = kzalloc(sizeof(struct npu_memory_debug_info), GFP_KERNEL);
	if (!dbg_info) {
		npu_err("there is no memory for npu_memory_debug_info\n");
		goto p_err;
	}
	dbg_info->dma_buf = (u64)dmabuf;
	dbg_info->size = size;
	dbg_info->pid = task_pid_nr(current);
	strncpy(dbg_info->comm, current->comm, TASK_COMM_LEN);
	atomic64_set(&dbg_info->ref_count, atomic64_read(&dmabuf->file->f_count));
	list_add_tail(&dbg_info->list, &memory->info_list);
	memory->info_count++;
done:
p_err:
	spin_unlock_irqrestore(&memory->info_lock, flags);
}

void npu_memory_update_dbg_info(struct npu_memory *memory, struct dma_buf *dmabuf)
{
	unsigned long flags;
	struct npu_memory_debug_info *info, *temp;

	if (!memory) {
		npu_info("npu_memory is NULL, skip this func.\n");
		return;
	}

	spin_lock_irqsave(&memory->info_lock, flags);
	list_for_each_entry_safe(info, temp, &memory->info_list, list) {
		if (info->dma_buf != (u64)dmabuf)
			continue;

		if (atomic64_read(&dmabuf->file->f_count) > 2) {
			atomic64_set(&info->ref_count, atomic64_read(&dmabuf->file->f_count));
			continue;
		}

		memory->info_count--;
		list_del(&info->list);
		kfree(info);
	}
	spin_unlock_irqrestore(&memory->info_lock, flags);
}

void npu_memory_unmap(struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	unsigned long flags;

	BUG_ON(!memory);
	BUG_ON(!buffer);

	npu_info("Try unmapping DMABUF fd=%d size=%zu\n", buffer->m.fd, buffer->size);
	npu_trace("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attachment[%pK]\n",
			buffer, buffer->vaddr, buffer->daddr, buffer->sgt, buffer->attachment);

	if (!IS_ERR_OR_NULL(buffer->vaddr))
		npu_dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
	if (buffer->daddr && !IS_ERR_VALUE(buffer->daddr))
		npu_memory_dma_buf_dva_unmap(buffer, false);
	if (!IS_ERR_OR_NULL(buffer->sgt))
		dma_buf_unmap_attachment(buffer->attachment, buffer->sgt, DMA_BIDIRECTIONAL);
	if (!IS_ERR_OR_NULL(buffer->attachment) && !IS_ERR_OR_NULL(buffer->dma_buf))
		dma_buf_detach(buffer->dma_buf, buffer->attachment);
	if (!IS_ERR_OR_NULL(buffer->dma_buf)) {
		dma_buf_put(buffer->dma_buf);
		npu_memory_update_dbg_info(memory, buffer->dma_buf);
	}

	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;


	spin_lock_irqsave(&memory->map_lock, flags);
	if (likely(!list_empty(&buffer->list))) {
		list_del(&buffer->list);
		INIT_LIST_HEAD(&buffer->list);
		memory->map_count--;
	} else
		npu_info("buffer[%pK] is not linked to map_lock. Skipping remove.\n", buffer);

	spin_unlock_irqrestore(&memory->map_lock, flags);
}

int __npu_memory_alloc(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot,
		struct dma_buf *dma_buf)
{
	int ret = 0;
	bool complete_suc = false;

	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t daddr;
	void *vaddr;

	unsigned long flags = 0;

	attachment = dma_buf_attach(dma_buf, memory->dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		npu_err("dma_buf_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->attachment = attachment;

	if (prot)
		npu_dma_buf_add_attr(attachment);

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		npu_err("dma_buf_map_attach is fail(%d) size(%zu)\n",
			ret, buffer->size);
		goto p_err;
	}
	buffer->sgt = sgt;

	daddr = npu_memory_dma_buf_dva_map(buffer);
	if (IS_ERR_VALUE(daddr)) {
		npu_err("fail(err %pad) to allocate iova\n", &daddr);
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->daddr = daddr;

	vaddr = npu_dma_buf_vmap(dma_buf);
	if (IS_ERR(vaddr)) {
		npu_err("fail(err %pK) in dma_buf_vmap\n", vaddr);
		ret = -EFAULT;
		goto p_err;
	}
	buffer->vaddr = vaddr;

	complete_suc = true;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	list_add_tail(&buffer->list, &memory->alloc_list);
	memory->alloc_count++;
	spin_unlock_irqrestore(&memory->alloc_lock, flags);

	npu_dbg("buffer[%pK], paddr[%llx], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
		buffer, buffer->paddr, buffer->vaddr, buffer->daddr, buffer->sgt, buffer->attachment, buffer->size);
p_err:
	if (complete_suc != true) {
		npu_memory_free(memory, buffer);
		npu_memory_dump(memory);
	}
	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
int npu_memory_alloc_secure(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot)
{
	int ret = 0;
	int flag = 0;
	int size;
	struct dma_buf *dma_buf;

	BUG_ON(!memory);
	BUG_ON(!buffer);
	if (!buffer->size)
		return -1;

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
	INIT_LIST_HEAD(&buffer->list);

	size = buffer->size;

	dma_buf = npu_memory_ion_alloc_secure(size, flag);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("npu_memory_ion_alloc_secure is fail(%pK) size(%zu)\n",
				dma_buf, buffer->size);
		ret = -EINVAL;
		return ret;
	}
	buffer->dma_buf = dma_buf;

	return __npu_memory_alloc(memory, buffer, prot, dma_buf);
}
#endif

int npu_memory_alloc(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot)
{
	int ret = 0;
	struct dma_buf *dma_buf;
	unsigned int flag = 0;
	size_t size;

	BUG_ON(!memory);
	BUG_ON(!buffer);
	if (!buffer->size)
		return 0;

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
	INIT_LIST_HEAD(&buffer->list);

	size = buffer->size;

	dma_buf = npu_memory_ion_alloc(size, flag);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("npu_memory_ion_alloc is fail(%pK) size(%zu)\n",
				dma_buf, buffer->size);
		ret = -EINVAL;
		npu_memory_dump(memory);
		return ret;
	}
	buffer->dma_buf = dma_buf;

	return __npu_memory_alloc(memory, buffer, prot, dma_buf);
}

int npu_memory_alloc_cached(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot)
{
	struct dma_buf *dma_buf;
	unsigned int flag = 0;
	size_t size = buffer->size;;

	if (!buffer->size)
		return 0; /* Nothing to do. */

	INIT_LIST_HEAD(&buffer->list);

	dma_buf = npu_memory_ion_alloc_cached(size, flag);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("cached heap allocation failed(%pK) size(%zu)\n",
			dma_buf, buffer->size);
		npu_memory_dump(memory);
		return -EINVAL;
	}

	buffer->dma_buf = dma_buf;

	return __npu_memory_alloc(memory, buffer, prot, dma_buf);
}

int npu_memory_free(struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	unsigned long flags;

	BUG_ON(!memory);
	BUG_ON(!buffer);
	if (!buffer->size)
		return 0;

	npu_trace("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attachment[%pK]\n",
		buffer, buffer->vaddr, buffer->daddr, buffer->sgt, buffer->attachment);

	if (!IS_ERR_OR_NULL(buffer->vaddr))
		npu_dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
	if (buffer->daddr && !IS_ERR_VALUE(buffer->daddr))
		npu_memory_dma_buf_dva_unmap(buffer, false);
	if (!IS_ERR_OR_NULL(buffer->sgt))
		dma_buf_unmap_attachment(buffer->attachment, buffer->sgt, DMA_BIDIRECTIONAL);
	if (!IS_ERR_OR_NULL(buffer->attachment) && !IS_ERR_OR_NULL(buffer->dma_buf))
		dma_buf_detach(buffer->dma_buf, buffer->attachment);
	if (!IS_ERR_OR_NULL(buffer->dma_buf))
		dma_buf_put(buffer->dma_buf);

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	if (likely(!list_empty(&buffer->list))) {
		list_del(&buffer->list);
		INIT_LIST_HEAD(&buffer->list);
		memory->alloc_count--;
	} else
		npu_info("buffer[%pK] is not linked to alloc_lock. Skipping remove.\n", buffer);

	spin_unlock_irqrestore(&memory->alloc_lock, flags);

	return 0;
}

/**
 * npu_memory_copy - Create a copy of NPU mem dmabuf.
 * @memory: Session memory manager.
 * @buffer: memory buffer.
 * @offset: Start offset in source buffer.
 * @size: Buffer size to copy.
 */
struct npu_memory_buffer *
npu_memory_copy(struct npu_memory *memory, struct npu_memory_buffer *buffer, size_t offset, size_t size)
{
	struct npu_memory_buffer *copy_buffer;
	void *vaddr;
	int ret;

	if (!buffer->dma_buf) {
		npu_err("no dmabuf in source buffer\n");
		return ERR_PTR(-EINVAL);
	}

	if (offset >= buffer->dma_buf->size || offset + size > buffer->dma_buf->size) {
		npu_err("invalid size args. size=0x%lx, offset=0x%lx, src size=0x%lx\n",
			size, offset, buffer->size);
		return ERR_PTR(-EINVAL);
	}

	copy_buffer = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (!copy_buffer)
		return ERR_PTR(-ENOMEM);

	/* Copy source buffer properties. */
	memcpy(copy_buffer, buffer, sizeof(*buffer));

	copy_buffer->size = size;

	/*
	 * We do memcpy from user NCP header to copy_buffer.
	 * Hence, allocate cached dma-buf heap.
	 */
	ret = npu_memory_alloc_cached(memory, copy_buffer, npu_get_configs(NPU_PBHA_HINT_00));
	if (ret) {
		npu_err("failed to allocate copy buffer(%d)\n", ret);
		goto err_free_mem;
	}

	/* Create virtual mapping for CPU access. */
	if (!buffer->vaddr) {
		vaddr = npu_dma_buf_vmap(buffer->dma_buf);
		if (IS_ERR_OR_NULL(vaddr)) {
			npu_err("failed to vmap original dmabuf(%ld)", PTR_ERR(vaddr));
			goto err_free_copy_buffer;
		}

		buffer->vaddr = vaddr;
	}

	vaddr = buffer->vaddr + offset;

	/* Begin CPU access. */
	dma_buf_begin_cpu_access(buffer->dma_buf, DMA_BIDIRECTIONAL);
	dma_buf_begin_cpu_access(copy_buffer->dma_buf, DMA_BIDIRECTIONAL);

	memcpy(copy_buffer->vaddr, vaddr, size);

	/* End CPU access.. */
	dma_buf_end_cpu_access(copy_buffer->dma_buf, DMA_BIDIRECTIONAL);
	dma_buf_end_cpu_access(buffer->dma_buf, DMA_BIDIRECTIONAL);

	return copy_buffer;
err_free_copy_buffer:
	npu_memory_free(memory, copy_buffer);
err_free_mem:
	kfree(copy_buffer);
	return ERR_PTR(ret);
}

int npu_memory_v_alloc(struct npu_memory *memory, struct npu_memory_v_buf *buffer)
{
	unsigned long flags = 0;

	BUG_ON(!memory);
	BUG_ON(!buffer);

	buffer->v_buf = (char *)vmalloc((unsigned long)buffer->size);
	if (!buffer->v_buf) {
		npu_err("failed vmalloc\n");
		npu_memory_dump(memory);
		return -ENOMEM;
	}

	spin_lock_irqsave(&memory->valloc_lock, flags);
	list_add_tail(&buffer->list, &memory->valloc_list);
	memory->valloc_count++;
	spin_unlock_irqrestore(&memory->valloc_lock, flags);

	return 0;
}

#ifndef CONFIG_NPU_KUNIT_TEST
void npu_memory_v_free(struct npu_memory *memory, struct npu_memory_v_buf *buffer)
{
	unsigned long flags = 0;

	BUG_ON(!memory);
	BUG_ON(!buffer);

	vfree((void *)buffer->v_buf);

	spin_lock_irqsave(&memory->valloc_lock, flags);
	if (likely(!list_empty(&buffer->list))) {
		list_del(&buffer->list);
		INIT_LIST_HEAD(&buffer->list);
		memory->valloc_count--;
	} else
		npu_info("buffer[%pK] is not linked to alloc_lock. Skipping remove.\n", buffer);

	spin_unlock_irqrestore(&memory->valloc_lock, flags);

	return;
}

int npu_memory_secure_alloc(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot)
{
	//TODO make secure alloc
	return npu_memory_alloc(memory, buffer, prot);
}

int npu_memory_secure_free(struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	//TODO make secure free
	return npu_memory_free(memory, buffer);
}
#endif

void * npu_dma_buf_vmap(struct dma_buf *dmabuf)
{
	void *vaddr;
	int ret = 0;
	struct iosys_map map;

	memset(&map, 0, sizeof(map));
	ret = dma_buf_vmap(dmabuf, &map);
	if (unlikely(ret)) {
		vaddr = ERR_PTR(ret);
	} else {
		vaddr = map.vaddr;
	}
	return vaddr;
}

void npu_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(vaddr);
	dma_buf_vunmap(dmabuf, &map);
}

MODULE_IMPORT_NS(DMA_BUF);

int npu_memory_alloc_from_heap(struct platform_device *pdev,
		struct npu_memory_buffer *buffer, dma_addr_t daddr,
		struct npu_memory *memory, const char *heapname, int prot)
{
	int ret = 0;
	bool complete_suc = false;
	unsigned long flags;

	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	phys_addr_t phys_addr;
	void *vaddr;
	int flag;

	size_t size;
	size_t map_size = 0;
	unsigned long iommu_attributes = 0;

	BUG_ON(!buffer);

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
	buffer->npu_sgt = false;
	strncpy(buffer->name, heapname, strlen(heapname));
	INIT_LIST_HEAD(&buffer->list);

	size = buffer->size;
	flag = 0;

	if (daddr) {
		struct iommu_domain *domain = iommu_get_domain_for_dev(&pdev->dev);

		ret = npu_memory_create_sgt(buffer, size);
		if (ret) {
			probe_err("fail(err %d) in npu_memory_create_sgt\n", ret);
			ret = -ENOMEM;
			goto p_err;
		}

		mb();
		npu_sgt_flush(buffer->sgt);

		if (prot)
			iommu_attributes = ((prot) << IOMMU_PRIV_SHIFT);

		if (likely(domain)) {
			map_size = iommu_map_sgtable(domain, daddr, buffer->sgt, iommu_attributes);
		} else {
			probe_err("fail(err %pad) in iommu_map_sg\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}

		buffer->daddr = daddr;

		if (map_size != size) {
			probe_info("iommu_map_sgtable return %lu, we want to map %lu",
				map_size, size);
		}

		ret = npu_memory_vmap(buffer, size);
		if (ret)
			probe_err("fail(err %d) in npu_memory_vmap\n", ret);

		buffer->npu_sgt = true;
		npu_memory_trace_for_vh(buffer->size, true);
	} else {
		dma_buf = npu_memory_ion_alloc(size, flag);
		if (IS_ERR_OR_NULL(dma_buf)) {
			probe_err("npu_memory_ion_alloc is fail(%p)\n", dma_buf);
			ret = -EINVAL;
			goto p_err;
		}
		buffer->dma_buf = dma_buf;

		attachment = dma_buf_attach(buffer->dma_buf, &pdev->dev);
		if (IS_ERR(attachment)) {
			ret = PTR_ERR(attachment);
			probe_err("dma_buf_attach is fail(%d)\n", ret);
			goto p_err;
		}
		buffer->attachment = attachment;

		if (prot)
			iommu_attributes = npu_memory_set_prot(prot, attachment);

		sgt = dma_buf_map_attachment(buffer->attachment, DMA_BIDIRECTIONAL);
		if (IS_ERR(sgt)) {
			ret = PTR_ERR(sgt);
			probe_err("dma_buf_map_attach is fail(%d)\n", ret);
			goto p_err;
		}
		buffer->sgt = sgt;

		daddr = npu_memory_dma_buf_dva_map(buffer);
		if (IS_ERR_VALUE(daddr)) {
			probe_err("fail(err %pad) to allocate iova\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}
		buffer->daddr = daddr;

		vaddr = npu_dma_buf_vmap(dma_buf);
		if (IS_ERR(vaddr) || !vaddr) {
			if (vaddr)
				probe_err("fail(err %p) in dma_buf_vmap\n", vaddr);
			else /* !vaddr */
				probe_err("fail(err NULL) in dma_buf_vmap\n");

			ret = -EFAULT;
			goto p_err;
		}
		buffer->vaddr = vaddr;
	}

	complete_suc = true;

	/* But, first phys. Not all */
	phys_addr = sg_phys(buffer->sgt->sgl);
	buffer->paddr = phys_addr;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	list_add_tail(&buffer->list, &memory->alloc_list);
	memory->alloc_count++;
	spin_unlock_irqrestore(&memory->alloc_lock, flags);
p_err:
	if (complete_suc != true) {
		npu_memory_free_from_heap(&pdev->dev, memory, buffer);
		npu_memory_dump(memory);
	}
	return ret;
}

void npu_memory_free_from_heap(struct device *dev, struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	unsigned long flags;
	struct iommu_domain *domain = iommu_get_domain_for_dev(dev);

	if (buffer->npu_sgt) {
		if (buffer->vaddr)
			vunmap(buffer->vaddr);
		if (buffer->daddr && !IS_ERR_VALUE(buffer->daddr))
			iommu_unmap(domain, buffer->daddr, buffer->size);
		if (buffer->sgt)
			npu_memory_free_sgt(buffer->sgt);

		npu_memory_trace_for_vh(buffer->size, false);
	} else {
		if (buffer->vaddr)
			npu_dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
		if (buffer->daddr && !IS_ERR_VALUE(buffer->daddr))
			npu_memory_dma_buf_dva_unmap(buffer, true);
		if (buffer->sgt)
			dma_buf_unmap_attachment(buffer->attachment, buffer->sgt, DMA_BIDIRECTIONAL);
		if (buffer->attachment)
			dma_buf_detach(buffer->dma_buf, buffer->attachment);
		if (buffer->dma_buf)
			dma_buf_put(buffer->dma_buf);
	}

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	if (likely(!list_empty(&buffer->list))) {
		list_del(&buffer->list);
		INIT_LIST_HEAD(&buffer->list);
		memory->alloc_count--;
	} else
		npu_info("buffer[%pK] is not linked to alloc_lock. Skipping remove.\n", buffer);

	spin_unlock_irqrestore(&memory->alloc_lock, flags);
}

MODULE_IMPORT_NS(DMA_BUF);
