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
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/dma-heap.h>

#include "common/npu-log.h"
#include "npu-binary.h"
#include "npu-memory.h"
#include "npu-config.h"

static struct dma_buf *npu_memory_ion_alloc(size_t size, unsigned int flag)
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

static dma_addr_t npu_memory_dma_buf_dva_map(struct npu_memory_buffer *buffer)
{
	return sg_dma_address(buffer->sgt->sgl);
}

static void npu_memory_dma_buf_dva_unmap(
		__attribute__((unused))struct npu_memory_buffer *buffer)
{
	return;
}

static void npu_dma_set_mask(struct device *dev)
{
	dma_set_mask(dev, DMA_BIT_MASK(32));
}

static void npu_dma_buf_add_attr(struct dma_buf_attachment *attachment)
{
	attachment->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
}

int npu_memory_probe(struct npu_memory *memory, struct device *dev)
{
	BUG_ON(!memory);
	BUG_ON(!dev);

	memory->dev = dev;

	atomic_set(&memory->refcount, 0);

	npu_dma_set_mask(dev);

	spin_lock_init(&memory->map_lock);
	INIT_LIST_HEAD(&memory->map_list);
	memory->map_count = 0;

	spin_lock_init(&memory->alloc_lock);
	INIT_LIST_HEAD(&memory->alloc_list);
	memory->alloc_count = 0;

	spin_lock_init(&memory->valloc_lock);
	INIT_LIST_HEAD(&memory->valloc_list);
	memory->valloc_count = 0;

	return 0;
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
		npu_dump("------------------------------------------------\n");
	}
	spin_unlock_irqrestore(&memory->map_lock, flags);
}
#endif

int npu_memory_map(struct npu_memory *memory, struct npu_memory_buffer *buffer, int prot)
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

	npu_info("Try mapping DMABUF fd=%d size=%zu\n", buffer->fd, buffer->size);
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
	INIT_LIST_HEAD(&buffer->list);

	dma_buf = dma_buf_get(buffer->fd);
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
p_err:
	if (complete_suc != true) {
		npu_memory_unmap(memory, buffer);
		npu_memory_dump(memory);
	}
	return ret;
}

void npu_memory_unmap(struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	unsigned long flags;

	BUG_ON(!memory);
	BUG_ON(!buffer);

	npu_info("Try unmapping DMABUF fd=%d size=%zu\n", buffer->fd, buffer->size);
	npu_trace("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attachment[%pK]\n",
			buffer, buffer->vaddr, buffer->daddr, buffer->sgt, buffer->attachment);

	if (!IS_ERR_OR_NULL(buffer->vaddr))
		npu_dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
	if (buffer->daddr && !IS_ERR_VALUE(buffer->daddr))
		npu_memory_dma_buf_dva_unmap(buffer);
	if (!IS_ERR_OR_NULL(buffer->sgt))
		dma_buf_unmap_attachment(buffer->attachment, buffer->sgt, DMA_BIDIRECTIONAL);
	if (!IS_ERR_OR_NULL(buffer->attachment) && !IS_ERR_OR_NULL(buffer->dma_buf))
		dma_buf_detach(buffer->dma_buf, buffer->attachment);
	if (!IS_ERR_OR_NULL(buffer->dma_buf))
		dma_buf_put(buffer->dma_buf);

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
	const struct vb2_mem_ops *mem_ops;
	unsigned long flags;

	BUG_ON(!memory);
	BUG_ON(!buffer);
	if (!buffer->size)
		return 0;

	mem_ops = memory->vb2_mem_ops;

	npu_trace("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attachment[%pK]\n",
		buffer, buffer->vaddr, buffer->daddr, buffer->sgt, buffer->attachment);

	if (!IS_ERR_OR_NULL(buffer->vaddr))
		npu_dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
	if (buffer->daddr && !IS_ERR_VALUE(buffer->daddr))
		npu_memory_dma_buf_dva_unmap(buffer);
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
	int ret = 0;
	struct dma_buf_map map;

	memset(&map, 0, sizeof(map));
	ret = dma_buf_vmap(dmabuf, &map);
	if (unlikely(ret)) {
		vaddr = ERR_PTR(ret);
	} else {
		vaddr = map.vaddr;
	}
#else
	vaddr = dma_buf_vmap(dmabuf);
#endif
	return vaddr;
}

void npu_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(vaddr);
	dma_buf_vunmap(dmabuf, &map);
#else
	dma_buf_vunmap(dmabuf, vaddr);
#endif

}
