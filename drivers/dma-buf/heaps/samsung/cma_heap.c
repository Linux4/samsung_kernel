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
	struct mutex utc_lock;
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

static struct dma_buf *cma_heap_allocate(struct dma_heap *heap, unsigned long len,
					 unsigned long fd_flags, unsigned long heap_flags)
{
	struct samsung_dma_heap *samsung_dma_heap = dma_heap_get_drvdata(heap);
	struct cma_heap *cma_heap = samsung_dma_heap->priv;
	struct samsung_dma_buffer *buffer;
	struct dma_buf *dmabuf;
	struct page *pages;
	unsigned int alignment = samsung_dma_heap->alignment;
	unsigned long size, nr_pages;
	void *priv = NULL;
	int protret = 0, ret = -ENOMEM;

	dma_heap_event_begin();

	if (dma_heap_flags_video_aligned(samsung_dma_heap->flags))
		len = dma_heap_add_video_padding(len);

	size = ALIGN(len, alignment);
	nr_pages = size >> PAGE_SHIFT;

	buffer = samsung_dma_buffer_alloc(samsung_dma_heap, size, 1);
	if (IS_ERR(buffer))
		return ERR_PTR(-ENOMEM);

	if (!dma_heap_flags_untouchable(samsung_dma_heap->flags)) {
		pages = cma_alloc(cma_heap->cma, nr_pages, get_order(alignment), 0);
		if (!pages) {
			perrfn("failed to allocate from %s, size %lu", dma_heap_get_name(heap), size);
			goto free_cma;
		}

		sg_set_page(buffer->sg_table.sgl, pages, size, 0);
		heap_page_clean(pages, size);
		heap_cache_flush(buffer);

		if (dma_heap_flags_protected(samsung_dma_heap->flags)) {
			priv = samsung_dma_buffer_protect(samsung_dma_heap, size, 1,
					page_to_phys(pages));
			if (IS_ERR(priv)) {
				ret = PTR_ERR(priv);
				goto free_prot;
			}
			buffer->priv = priv;
		}
	} else {
		struct page *alloc_pages;
		phys_addr_t paddr;

		mutex_lock(&cma_heap->utc_lock);
		if (++cma_heap->alloc_count == 1) {
			unsigned long heap_size = cma_heap->alloc_size;

			nr_pages = heap_size >> PAGE_SHIFT;
			cma_heap->pages = pages = cma_alloc(cma_heap->cma, nr_pages,
							    get_order(alignment), 0);
			if (!cma_heap->pages) {
				perrfn("failed to allocate untouchable from %s, size %lu",
				       dma_heap_get_name(heap), heap_size);
				goto free_cma;
			}

			cma_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
			if (!cma_heap->pool) {
				perrfn("failed to create pool");
				goto free_prot;
			}

			ret = gen_pool_add(cma_heap->pool, page_to_phys(pages), heap_size, -1);
			if (ret) {
				perrfn("failed to add pool from %s, base %lu, size %lu",
				       dma_heap_get_name(heap), page_to_phys(pages), heap_size);
				gen_pool_destroy(cma_heap->pool);
				goto free_prot;
			}

			paddr = gen_pool_alloc(cma_heap->pool, size);
			if (!paddr) {
				perrfn("failed to allocate 1st pool from %s, size %lu",
				       dma_heap_get_name(heap), size);
				gen_pool_destroy(cma_heap->pool);
				goto free_prot;
			}

			alloc_pages = phys_to_page(paddr);
			sg_set_page(buffer->sg_table.sgl, alloc_pages, size, 0);

			heap_page_clean(cma_heap->pages, heap_size);
			cma_heap_region_flush(dma_heap_get_dev(buffer->heap->dma_heap),
					      alloc_pages, heap_size);
			if (dma_heap_flags_protected(samsung_dma_heap->flags)) {
				priv = samsung_dma_buffer_protect(samsung_dma_heap, heap_size, 1,
						page_to_phys(cma_heap->pages));
				if (IS_ERR(priv)) {
					ret = PTR_ERR(priv);
					gen_pool_free(cma_heap->pool, sg_phys(buffer->sg_table.sgl),
						      buffer->len);
					gen_pool_destroy(cma_heap->pool);
					goto free_prot;
				}
				cma_heap->priv = priv;
				buffer->priv = samsung_dma_buffer_copy_offset_info(priv, 0);
			}
		} else {
			size_t offset;

			paddr = gen_pool_alloc(cma_heap->pool, size);
			if (!paddr) {
				perrfn("failed to allocate pool from %s, size %lu",
				       dma_heap_get_name(heap), size);
				goto free_cma;
			}

			alloc_pages = phys_to_page(paddr);
			sg_set_page(buffer->sg_table.sgl, alloc_pages, size, 0);
			offset = paddr - page_to_phys(cma_heap->pages);
			buffer->priv = samsung_dma_buffer_copy_offset_info(cma_heap->priv, offset);
		}
		mutex_unlock(&cma_heap->utc_lock);
	}

	dmabuf = samsung_export_dmabuf(buffer, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_export;
	}

	dma_heap_event_record(DMA_HEAP_EVENT_ALLOC, dmabuf, begin);

	return dmabuf;

free_export:
	protret = samsung_dma_buffer_unprotect(priv, samsung_dma_heap);
free_prot:
	if (!protret)
		cma_release(cma_heap->cma, pages, nr_pages);
free_cma:
	samsung_dma_buffer_free(buffer);

	if (dma_heap_flags_untouchable(samsung_dma_heap->flags)) {
		cma_heap->alloc_count--;
		if (cma_heap->alloc_count == 0)
			cma_heap->priv = NULL;
		mutex_unlock(&cma_heap->utc_lock);
	}

	samsung_allocate_error_report(samsung_dma_heap, len, fd_flags, heap_flags);

	return ERR_PTR(ret);
}

static void cma_heap_release(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *dma_heap = buffer->heap;
	struct cma_heap *cma_heap = dma_heap->priv;
	unsigned long nr_pages = buffer->len >> PAGE_SHIFT;
	int ret = 0;

	if (!dma_heap_flags_untouchable(dma_heap->flags)) {
		if (dma_heap_flags_protected(dma_heap->flags))
			ret = samsung_dma_buffer_unprotect(buffer->priv, dma_heap);

		if (!ret)
			cma_release(cma_heap->cma, sg_page(buffer->sg_table.sgl), nr_pages);
	} else {
		mutex_lock(&cma_heap->utc_lock);
		if (--cma_heap->alloc_count == 0) {
			unsigned long heap_size = cma_heap->alloc_size;

			if (dma_heap_flags_protected(buffer->flags))
				ret = samsung_dma_buffer_unprotect(cma_heap->priv, dma_heap);
			if (!ret) {
				cma_release(cma_heap->cma, cma_heap->pages,
					    heap_size >> PAGE_SHIFT);
				cma_heap->priv = NULL;
			}
		}
		if (!ret) {
			gen_pool_free(cma_heap->pool, sg_phys(buffer->sg_table.sgl), buffer->len);
			if (cma_heap->alloc_count == 0)
				gen_pool_destroy(cma_heap->pool);
		}
		mutex_unlock(&cma_heap->utc_lock);

		samsung_dma_buffer_release_copied_info(buffer->priv);
	}

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
	mutex_init(&cma_heap->utc_lock);

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
