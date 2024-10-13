// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cma.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

#include "pablo-icpu.h"
#include "pablo-icpu-mem-wrapper.h"
#include "pablo-debug.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-CMA_WRAP]",
};

struct icpu_logger *get_icpu_cma_log(void)
{
	return &_log;
}

struct icpu_cma_buf {
	size_t size;

	struct sg_table                 *sgt;
	dma_addr_t                      daddr;
	dma_addr_t                      iova;
	phys_addr_t                     paddr;
	void                            *vaddr;

	size_t                          map_size;

	struct cma                      *cma_area;

	void *priv;
};

static struct device sync_dev;

int __iommu_map_cma(struct device *dev, struct icpu_cma_buf *fw_buf)
{
	struct device_node *node = dev->of_node;
	dma_addr_t reserved_base;
	const __be32 *prop;

	prop = of_get_property(node, "samsung,iommu-reserved-map", NULL);
	if (!prop) {
		ICPU_ERR("No reserved F/W dma area");
		return -ENOENT;
	}

	reserved_base = of_read_number(prop, of_n_addr_cells(node));

	if (!reserved_base) {
		ICPU_ERR("There is no firmware reserved_base");
		return -ENOMEM;
	}

	fw_buf->map_size = iommu_map_sg(iommu_get_domain_for_dev(dev), reserved_base,
			fw_buf->sgt->sgl,
			fw_buf->sgt->orig_nents,
			IOMMU_READ|IOMMU_WRITE);
	if (!fw_buf->map_size) {
		ICPU_ERR("Failed to map iova (err VA: %#llx, PA: %#llx)\n",
				reserved_base, fw_buf->paddr);
		return -ENOMEM;
	}

	fw_buf->daddr = reserved_base;
	ICPU_INFO("[MEMINFO] FW mapped iova: %#llx (pa: %#llx)\n",
			fw_buf->daddr, fw_buf->paddr);

	return 0;
}

static int __cma_alloc(struct device *dev, struct icpu_buf *special_buf)
{
	struct page *fw_pages;
	unsigned long nr_pages;
	int ret;
	struct icpu_cma_buf *buf;
	unsigned int align;
	int retry_count = 3;

	if (!special_buf || !dev) {
		ICPU_ERR("invalid parameters");
		return -EINVAL;
	}

	buf = kzalloc(sizeof(struct icpu_cma_buf), GFP_KERNEL);
	if (!buf) {
		ICPU_ERR("Failed to allocate icpu_cma_buf");
		return -ENOMEM;
	}

	nr_pages = special_buf->size >> PAGE_SHIFT;
	align = special_buf->align ? get_order(special_buf->align) : 0;

	buf->size = special_buf->size;
	buf->cma_area = dev->cma_area;

	while(retry_count) {
		fw_pages = cma_alloc(buf->cma_area, nr_pages, align, false);
		if (fw_pages) {
			break;
		} else {
			ICPU_WARN("cma_alloc failed. retry(%d)", retry_count);
			retry_count--;
			usleep_range(1000, 1001);
		}
	}

	if (!fw_pages) {
		ICPU_ERR("Failed to allocate with CMA");

		goto err_buf_alloc;
	}

	buf->sgt = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!buf->sgt) {
		ICPU_ERR("Failed to allocate with kmalloc");
		goto err_cma_alloc;
	}

	ret = sg_alloc_table(buf->sgt, 1, GFP_KERNEL);
	if (ret) {
		ICPU_ERR("Failed to allocate sg_table");
		goto err_sg_alloc;
	}

	sg_set_page(buf->sgt->sgl, fw_pages, buf->size, 0);

	buf->paddr = page_to_phys(fw_pages);
	buf->vaddr = phys_to_virt(buf->paddr);

	ret = __iommu_map_cma(dev, buf);
	if (ret) {
		ICPU_ERR("iommu map cma fail");
		goto iommu_map_cma_fail;
	}

	buf->priv = dev;
	special_buf->data = buf;
	special_buf->paddr = buf->paddr;
	special_buf->daddr = buf->daddr;
	special_buf->vaddr = buf->vaddr;

	return 0;

iommu_map_cma_fail:
	sg_free_table(buf->sgt);
err_sg_alloc:
	kfree(buf->sgt);
	buf->sgt = NULL;
err_cma_alloc:
	cma_release(buf->cma_area, fw_pages, nr_pages);
err_buf_alloc:
	kfree(buf);

	return -ENOMEM;
}

static void __cma_free(struct icpu_buf *buf)
{
	struct icpu_cma_buf *rbuf;

	if (!buf || !buf->data)
		return;

	rbuf = buf->data;

	iommu_unmap(iommu_get_domain_for_dev(rbuf->priv), rbuf->daddr, rbuf->map_size);

	sg_free_table(rbuf->sgt);
	kfree(rbuf->sgt);

	cma_release(rbuf->cma_area, phys_to_page(rbuf->paddr),
			(rbuf->size >> PAGE_SHIFT));
	kfree(rbuf);
}

static void __cma_sync_for_device(struct icpu_buf *buf)
{
	if (!buf || !buf->data)
		return;

	dma_sync_single_for_device(&sync_dev, virt_to_phys(buf->vaddr),
			buf->size, DMA_TO_DEVICE);
}

struct icpu_buf_ops icpu_buf_ops_cma = {
	.alloc = __cma_alloc,
	.free = __cma_free,
	.sync_for_device = __cma_sync_for_device,
};

int pablo_icpu_cma_init(void *pdev)
{
	device_initialize(&sync_dev);

	return 0;
}

void pablo_icpu_cma_deinit(void)
{
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
void *pablo_kunit_alloc_dummy_cma_buf(void)
{
	struct icpu_buf *buf;

	buf = kzalloc(sizeof(struct icpu_buf), GFP_KERNEL);
	buf->type = 1;

	__cma_alloc(&sync_dev, buf);
	__iommu_map_cma(&sync_dev, buf->data);

	return buf;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_alloc_dummy_cma_buf);
#endif
