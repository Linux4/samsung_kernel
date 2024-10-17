// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF CMA heap exporter for Samsung
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#include <linux/cma.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-direct.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "heap_private.h"
#include "secure_buffer.h"

struct cma_heap {
	struct cma *cma;
	struct gen_pool *pool;
	struct reserved_mem *rmem;
	struct mutex prealloc_lock;
	struct page *pages;
	void *priv;
	int alloc_count;
	unsigned long alloc_size;
};

static void cma_heap_region_flush(struct device *dev, struct page *page, size_t size)
{
	dma_map_single(dev, page_to_virt(page), size, DMA_TO_DEVICE);
	dma_unmap_single(dev, phys_to_dma(dev, page_to_phys(page)), size, DMA_FROM_DEVICE);
}

static int preallocated_chunk_heap_init(struct dma_heap *heap, struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *samsung_dma_heap = dma_heap_get_drvdata(heap);
	struct cma_heap *cma_heap = samsung_dma_heap->priv;
	unsigned int alignment = samsung_dma_heap->alignment;
	unsigned long heap_size = cma_heap->alloc_size;
	unsigned long nr_pages = heap_size >> PAGE_SHIFT;
	int ret = -ENOMEM;

	cma_heap->pages = cma_alloc(cma_heap->cma, nr_pages, get_order(alignment), 0);
	if (!cma_heap->pages) {
		perrfn("failed to allocate preallocated chunk heap from %s, size %lu",
		       dma_heap_get_name(heap), heap_size);
		goto err_cma_alloc;
	}

	heap_page_clean(cma_heap->pages, heap_size);
	cma_heap_region_flush(dma_heap_get_dev(buffer->heap->dma_heap), cma_heap->pages, heap_size);

	cma_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!cma_heap->pool) {
		perrfn("failed to create pool");
		goto err_create_genpool;
	}

	ret = gen_pool_add(cma_heap->pool, page_to_phys(cma_heap->pages), heap_size, -1);
	if (ret) {
		perrfn("failed to add pool from %s, base %lx size %lu", dma_heap_get_name(heap),
		       (unsigned long)page_to_phys(cma_heap->pages), heap_size);
		goto err_add_genpool;
	}

	if (dma_heap_flags_protected(samsung_dma_heap->flags)) {
		void *priv = samsung_dma_buffer_protect(samsung_dma_heap, heap_size, 1,
							page_to_phys(cma_heap->pages));
		if (IS_ERR(priv)) {
			ret = PTR_ERR(priv);
			goto err_add_genpool;
		}
		cma_heap->priv = priv;
	}

	return 0;

err_add_genpool:
	gen_pool_destroy(cma_heap->pool);
err_create_genpool:
	cma_release(cma_heap->cma, cma_heap->pages, nr_pages);
err_cma_alloc:
	return ret;
}

static void preallocated_chunk_heap_deinit(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *samsung_dma_heap = buffer->heap;
	struct cma_heap *cma_heap = samsung_dma_heap->priv;
	unsigned long heap_size = cma_heap->alloc_size;
	int protret = 0;

	if (dma_heap_flags_protected(buffer->flags))
		protret = samsung_dma_buffer_unprotect(cma_heap->priv, samsung_dma_heap);

	gen_pool_destroy(cma_heap->pool);

	if (!protret)
		cma_release(cma_heap->cma, cma_heap->pages, heap_size >> PAGE_SHIFT);
}

static struct page *allocate_from_genpool(struct dma_heap *heap, struct cma_heap *cma_heap,
					  struct samsung_dma_buffer *buffer, unsigned long size)
{
	phys_addr_t paddr;
	struct page *alloc_pages;

	paddr = gen_pool_alloc(cma_heap->pool, size);
	if (!paddr) {
		perrfn("failed to allocate pool from %s, size %lu",
		       dma_heap_get_name(heap), size);
		return NULL;
	}
	alloc_pages = phys_to_page(paddr);
	sg_set_page(buffer->sg_table.sgl, alloc_pages, size, 0);

	return alloc_pages;
}

static int preallocated_chunk_heap_allocate(struct dma_heap *heap,
					    struct samsung_dma_buffer *buffer, unsigned long size)
{
	struct samsung_dma_heap *samsung_dma_heap = dma_heap_get_drvdata(heap);
	struct cma_heap *cma_heap = samsung_dma_heap->priv;
	struct page *alloc_pages;
	int ret;

	mutex_lock(&cma_heap->prealloc_lock);
	if (++cma_heap->alloc_count == 1) {
		ret = preallocated_chunk_heap_init(heap, buffer);
		if (ret)
			goto err_preallocated_init;
	}

	alloc_pages = allocate_from_genpool(heap, cma_heap, buffer, size);
	if (!alloc_pages) {
		ret = -ENOMEM;
		goto err_alloc_pages;
	}

	if (dma_heap_flags_protected(samsung_dma_heap->flags)) {
		size_t offset = page_to_phys(alloc_pages) - page_to_phys(cma_heap->pages);

		buffer->priv = samsung_dma_buffer_copy_offset_info(cma_heap->priv, offset);
		if (IS_ERR(buffer->priv)) {
			ret = PTR_ERR(buffer->priv);
			goto err_copy_protect;
		}
	}

	mutex_unlock(&cma_heap->prealloc_lock);
	return 0;

err_copy_protect:
	gen_pool_free(cma_heap->pool, sg_phys(buffer->sg_table.sgl), buffer->len);
err_alloc_pages:
	if (cma_heap->alloc_count == 1)
		preallocated_chunk_heap_deinit(buffer);
err_preallocated_init:
	cma_heap->alloc_count--;
	mutex_unlock(&cma_heap->prealloc_lock);
	return ret;
}

static void preallocated_chunk_heap_release(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *samsung_dma_heap = buffer->heap;
	struct cma_heap *cma_heap = samsung_dma_heap->priv;

	mutex_lock(&cma_heap->prealloc_lock);

	if (dma_heap_flags_protected(samsung_dma_heap->flags))
		samsung_dma_buffer_release_copied_info(buffer->priv);

	gen_pool_free(cma_heap->pool, sg_phys(buffer->sg_table.sgl), buffer->len);

	if (--cma_heap->alloc_count == 0)
		preallocated_chunk_heap_deinit(buffer);

	mutex_unlock(&cma_heap->prealloc_lock);

}

static int cma_heap_buffer_allocate(struct dma_heap *heap, struct samsung_dma_buffer *buffer,
				    unsigned long size)
{
	struct samsung_dma_heap *samsung_dma_heap = dma_heap_get_drvdata(heap);
	struct cma_heap *cma_heap = samsung_dma_heap->priv;
	unsigned long nr_pages = size >> PAGE_SHIFT;
	unsigned int alignment = samsung_dma_heap->alignment;
	struct page *pages;

	pages = cma_alloc(cma_heap->cma, nr_pages, get_order(alignment), 0);
	if (!pages) {
		perrfn("failed to allocate from %s, size %lu", dma_heap_get_name(heap), size);
		return -ENOMEM;
	}

	sg_set_page(buffer->sg_table.sgl, pages, size, 0);
	heap_page_clean(pages, size);
	heap_cache_flush(buffer);

	if (dma_heap_flags_protected(samsung_dma_heap->flags)) {
		void *priv = samsung_dma_buffer_protect(samsung_dma_heap, size, 1,
							page_to_phys(pages));
		if (IS_ERR(priv)) {
			cma_release(cma_heap->cma, pages, nr_pages);
			return PTR_ERR(priv);
		}
		buffer->priv = priv;
	}

	return 0;
}

static void cma_heap_buffer_release(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *samsung_dma_heap = buffer->heap;
	struct cma_heap *cma_heap = samsung_dma_heap->priv;
	unsigned long nr_pages = buffer->len >> PAGE_SHIFT;
	int protret = 0;

	if (dma_heap_flags_protected(samsung_dma_heap->flags))
		protret = samsung_dma_buffer_unprotect(buffer->priv, samsung_dma_heap);

	if (!protret)
		cma_release(cma_heap->cma, sg_page(buffer->sg_table.sgl), nr_pages);
}

static struct dma_buf *cma_heap_allocate(struct dma_heap *heap, unsigned long len,
					 unsigned long fd_flags, unsigned long heap_flags)
{
	struct samsung_dma_heap *samsung_dma_heap = dma_heap_get_drvdata(heap);
	struct samsung_dma_buffer *buffer;
	struct dma_buf *dmabuf;
	unsigned int alignment = samsung_dma_heap->alignment;
	unsigned long size;
	int ret;

	dma_heap_event_begin();

	if (dma_heap_flags_video_aligned(samsung_dma_heap->flags))
		len = dma_heap_add_video_padding(len);

	size = ALIGN(len, alignment);
	buffer = samsung_dma_buffer_alloc(samsung_dma_heap, size, 1);
	if (IS_ERR(buffer))
		return ERR_PTR(-ENOMEM);

	if (dma_heap_flags_preallocated(samsung_dma_heap->flags))
		ret = preallocated_chunk_heap_allocate(heap, buffer, size);
	else
		ret = cma_heap_buffer_allocate(heap, buffer, size);
	if (ret)
		goto err_alloc;

	dmabuf = samsung_export_dmabuf(buffer, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_export;
	}

	dma_heap_event_record(DMA_HEAP_EVENT_ALLOC, dmabuf, begin);

	return dmabuf;

free_export:
	if (dma_heap_flags_preallocated(samsung_dma_heap->flags))
		preallocated_chunk_heap_release(buffer);
	else
		cma_heap_buffer_release(buffer);
err_alloc:
	samsung_dma_buffer_free(buffer);
	samsung_allocate_error_report(samsung_dma_heap, len, fd_flags, heap_flags);

	return ERR_PTR(ret);
}

static void cma_heap_release(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *samsung_dma_heap = buffer->heap;

	if (dma_heap_flags_preallocated(samsung_dma_heap->flags))
		preallocated_chunk_heap_release(buffer);
	else
		cma_heap_buffer_release(buffer);

	samsung_dma_buffer_free(buffer);
}

static void rmem_remove_callback(void *p)
{
	of_reserved_mem_device_release((struct device *)p);
}

static const struct dma_heap_ops cma_heap_ops = {
	.allocate = cma_heap_allocate,
};

static int cma_heap_probe(struct platform_device *pdev)
{
	struct cma_heap *cma_heap;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	int ret;
	unsigned int alloc_size;

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!rmem_np) {
		perrdev(&pdev->dev, "no memory-region handle");
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		perrdev(&pdev->dev, "memory-region handle not found");
		return -ENODEV;
	}

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret || !pdev->dev.cma_area) {
		dev_err(&pdev->dev, "The CMA reserved area is not assigned (ret %d)\n", ret);
		return -EINVAL;
	}

	ret = devm_add_action(&pdev->dev, rmem_remove_callback, &pdev->dev);
	if (ret) {
		of_reserved_mem_device_release(&pdev->dev);
		return ret;
	}

	cma_heap = devm_kzalloc(&pdev->dev, sizeof(*cma_heap), GFP_KERNEL);
	if (!cma_heap)
		return -ENOMEM;

	ret = of_property_read_u32_index(pdev->dev.of_node, "dma-heap,alloc-size", 0, &alloc_size);
	if (ret)
		alloc_size = rmem->size;

	cma_heap->alloc_size = alloc_size;
	cma_heap->cma = pdev->dev.cma_area;
	cma_heap->rmem = rmem;
	mutex_init(&cma_heap->prealloc_lock);

	ret = samsung_heap_add(&pdev->dev, cma_heap, cma_heap_release, &cma_heap_ops);
	if (ret == -ENODEV)
		return 0;

	return ret;
}

static const struct of_device_id cma_heap_of_match[] = {
	{ .compatible = "samsung,dma-heap-cma", },
	{ },
};
MODULE_DEVICE_TABLE(of, cma_heap_of_match);

static struct platform_driver cma_heap_driver = {
	.driver		= {
		.name	= "samsung,dma-heap-cma",
		.of_match_table = cma_heap_of_match,
	},
	.probe		= cma_heap_probe,
};

int __init cma_dma_heap_init(void)
{
	return platform_driver_register(&cma_heap_driver);
}

void cma_dma_heap_exit(void)
{
	platform_driver_unregister(&cma_heap_driver);
}
