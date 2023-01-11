/*
 * Copyright 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <asm/cacheflush.h>
#include <linux/pm_runtime.h>
#ifdef CONFIG_ARM_SMMU
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <asm/dma-iommu.h>
#endif
#include "../ion.h"
#include "../ion_priv.h"
#include "pxa_ion.h"

#define VPU_DMABUF_META_ID	0x10000

struct pxa_ion_info {
	struct ion_device       *idev;
	struct ion_heap         **heaps;
	int                     heap_cnt;
};

u32 use_iommu = 1;
int num_extra_pages = 0;
struct page *page_pad;
struct ion_device *pxa_ion_dev;
EXPORT_SYMBOL(pxa_ion_dev);

#ifdef CONFIG_OF
static const struct of_device_id pxa_ion_dt_match[] = {
	{ .compatible = "marvell,pxa-ion" },
	{},
};
#endif

#ifdef CONFIG_ARM_SMMU
struct pxa_ion_iommu_meta {
	unsigned int iova;
	struct device *dev;
	struct sg_table sgtable;
};

static __init int setup_iommu(char *str)
{
	use_iommu = 0;
	return 0;
}
early_param("disable_iommu", setup_iommu);

struct dma_iommu_mapping *pxa_ion_iommu_mapping;
struct device *pxa_ion_platform_dev;
static int pxa_ion_register_iommu(struct device *dev)
{
	int ret;

	/* create iommu mapping for vpu */
	pxa_ion_iommu_mapping = arm_iommu_create_mapping(&platform_bus_type,
				0x40000000, 0x60000000, 0);
	if (IS_ERR(pxa_ion_iommu_mapping)) {
		dev_err(dev, "failed to create iommu mapping\n");
		return -1;
	}

	ret = arm_iommu_power_switch(dev, 1);
	if (!ret) {
		ret = arm_iommu_attach_device(dev, pxa_ion_iommu_mapping);
		if (ret < 0) {
			arm_iommu_power_switch(dev, 0);
			dev_err(dev, "failed in iommu attach.\n");
			return -ENOMEM;
		}
	} else {
		BUG_ON(1);
	}

	arm_iommu_power_switch(dev, 0);

	dev->dma_parms = devm_kzalloc(dev, sizeof(*dev->dma_parms),
					GFP_KERNEL);
	if (dev->dma_parms) {
		/*
		* set the max seg size to a max value, so as to use a single
		* iova for the whole seg list/chain
		*/
		dma_set_max_seg_size(dev, 0xffffffffu);
	} else {
		dev_err(dev, "failed to alloc dev->dma_parms\n");
		BUG_ON(1);
	}

	pxa_ion_platform_dev = dev;

	page_pad = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!page_pad) {
		dev_err(dev, "failed to alloc a pad page\n");
		BUG_ON(1);
	}

	return 0;
}

struct sg_table *sg_clone_table(struct sg_table *table_from,
	struct sg_table *table_to)
{
	struct scatterlist *sg_from, *sg_to;
	int i, ret = 0;

	/* alloc extra sg list for appendix buffer */
	ret = sg_alloc_table(table_to, table_from->nents + num_extra_pages,
			GFP_KERNEL);
	if (unlikely(ret))
		return NULL;
	sg_to = table_to->sgl;
	for_each_sg(table_from->sgl, sg_from, table_from->nents, i) {
		sg_to->page_link = sg_from->page_link;
		sg_to->offset = sg_from->offset;
		sg_to->length = sg_from->length;
		if (i == (table_from->nents - 1))
			sg_to->page_link = sg_to->page_link & ~0x02;
		sg_to = sg_next(sg_to);
	}

	/*
	 * FIX ME: always appendix some extra pages at the end of the buffer
	 * to avoid potential issue like DMA access across boundary
	 */
	if (sg_to) {
		for (i = 0; i < num_extra_pages; i++) {
			sg_assign_page(sg_to, page_pad);
			sg_to->offset = 0;
			sg_to->length = PAGE_SIZE;
			sg_to = sg_next(sg_to);
		}
	} else if (num_extra_pages) {
		pr_err("%s: appendixes page failure\n", __func__);
	}

	return table_to;
}

int pxa_ion_iommu_meta_release(void *priv)
{
	struct pxa_ion_iommu_meta *meta = (struct pxa_ion_iommu_meta *)priv;
	struct sg_table *st;
	struct scatterlist *sg;
	DEFINE_DMA_ATTRS(attrs);
	int ret;

	st = &(meta->sgtable);
	sg = st->sgl;

	ret = arm_iommu_power_switch(pxa_ion_platform_dev, 1);
	if (!ret) {
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
		dma_unmap_sg_attrs(meta->dev, sg, st->nents,
			DMA_BIDIRECTIONAL, &attrs);
		arm_iommu_power_switch(pxa_ion_platform_dev, 0);
	}

	sg_free_table(st);
	kfree(priv);
	return 0;
}

struct pxa_ion_iommu_meta *pxa_ion_iommu_map_dmabuf(struct device *dev,
	struct dma_buf *dmabuf)
{
	DEFINE_DMA_ATTRS(attrs);
	struct dma_buf_attachment *att;
	struct sg_table *st, *st_tmp;
	struct scatterlist *sg;
	struct pxa_ion_iommu_meta *meta;
	int ret;

	meta = kmalloc(sizeof(struct pxa_ion_iommu_meta), GFP_KERNEL);
	if (!meta)
		return NULL;

	att = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(att))
		goto err_attach;

	st = dma_buf_map_attachment(att, DMA_NONE);
	if (IS_ERR(st))
		goto err_map_attach;

	sg = st->sgl;
	st_tmp = sg_clone_table(st, &meta->sgtable);

	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	ret = dma_map_sg_attrs(dev, st_tmp->sgl, st_tmp->nents,
		DMA_BIDIRECTIONAL, &attrs);
	if (ret <= 0)
		goto err_map_sg;

	dma_buf_unmap_attachment(att, st, DMA_NONE);
	dma_buf_detach(dmabuf, att);
	dma_buf_meta_attach(dmabuf, VPU_DMABUF_META_ID, meta,
		pxa_ion_iommu_meta_release);

	meta->iova = meta->sgtable.sgl->dma_address;
	meta->dev = dev;
	return meta;

err_map_sg:
	sg_free_table(st_tmp);
	dma_buf_unmap_attachment(att, st, DMA_NONE);
err_map_attach:
	dma_buf_detach(dmabuf, att);
err_attach:
	kfree(meta);
	return NULL;
}
#endif

static long pxa_ion_ioctl(struct ion_client *client, unsigned int cmd,
							unsigned long arg)
{
	switch (cmd) {
#ifdef CONFIG_ARM_SMMU
	case ION_IOC_PHYS:
	{
		struct ion_phys_data *data = (struct ion_phys_data *)arg;
		struct dma_buf *dmabuf;
		struct pxa_ion_iommu_meta *meta;

		if (!pxa_ion_iommu_mapping)
			return -EFAULT;

		dmabuf = dma_buf_get(data->fd);
		if (IS_ERR_OR_NULL(dmabuf))
			return -EFAULT;

		meta = dma_buf_meta_fetch(dmabuf, VPU_DMABUF_META_ID);
		if (!meta)
			meta = pxa_ion_iommu_map_dmabuf(pxa_ion_platform_dev,
					dmabuf);
		if (meta)
			data->addr = meta->iova;
		else {
			pr_err("pxa ion: no dmabuf meta found\n");
			dma_buf_put(dmabuf);
			return -EFAULT;
		}

		dma_buf_put(dmabuf);
		break;
	}
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int pxa_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata;
	struct pxa_ion_info *info;
	struct ion_platform_heap *heaps;
	int err, i, nr, size;

	if (IS_ENABLED(CONFIG_OF)) {
		struct device_node *np = pdev->dev.of_node;
		struct device_node *child_np;

		if (!np)
			return -EINVAL;
		if (use_iommu) {
			of_property_read_u32(np, "marvell,ion-iommu",
					&use_iommu);
			of_property_read_u32(np, "marvell,ion-extra-pages",
					&num_extra_pages);
		}

		if (of_property_read_u32(np, "marvell,ion-nr", &nr))
			return -EINVAL;

		if (of_get_child_count(np) != nr)
			return -EINVAL;

		/* allocate heaps */
		size = sizeof(struct ion_platform_heap) * nr;

		heaps = kzalloc(size, GFP_KERNEL);
		if (!heaps)
			return -ENOMEM;

		i = 0;
		for_each_child_of_node(np, child_np) {
			u32 size32;

			if (of_property_read_u32(child_np, "marvell,ion-type",
						&((heaps + i)->type))) {
				err = -EINVAL;
				goto err_alloc;
			}
			if (of_property_read_u32(child_np, "marvell,ion-id",
						&((heaps + i)->id))) {
				err = -EINVAL;
				goto err_alloc;
			}
			if (of_property_read_string(child_np,
			    "marvell,ion-name", &((heaps + i)->name))) {
				err = -EINVAL;
				goto err_alloc;
			}
			of_property_read_u32(child_np, "marvell,ion-base",
						(u32 *)&((heaps + i)->base));
			of_property_read_u32(child_np, "marvell,ion-size",
						&size32);
			(heaps + i)->size = size32;
			i++;
		}
	} else {
		pdata = pdev->dev.platform_data;
		if (!pdata)
			return -EINVAL;
		nr = pdata->nr;
		heaps = pdata->heaps;
	}
	info = devm_kzalloc(&pdev->dev, sizeof(struct pxa_ion_info),
								GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto err_alloc;
	}

	info->heaps = devm_kzalloc(&pdev->dev,
		sizeof(struct ion_heap *) * nr, GFP_KERNEL);
	if (!info->heaps) {
		err = -ENOMEM;
		goto err_alloc;
	}

	info->heap_cnt = nr;

	info->idev = ion_device_create(pxa_ion_ioctl);
	if (IS_ERR_OR_NULL(info->idev)) {
		err = PTR_ERR(info->idev);
		goto err_alloc;
	}

	pxa_ion_dev = info->idev;

	/* create the heaps as specified in the board file */
	for (i = 0; i < nr; i++) {
		struct ion_platform_heap *heap_data = &heaps[i];

		info->heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(info->heaps[i])) {
			err = PTR_ERR(info->heaps[i]);
			goto err_heap;
		}
		ion_device_add_heap(info->idev, info->heaps[i]);
	}
	platform_set_drvdata(pdev, info);

#ifdef CONFIG_ARM_SMMU
	pm_runtime_enable(&pdev->dev);
	if (use_iommu)
		pxa_ion_register_iommu(&pdev->dev);
#endif

	if (IS_ENABLED(CONFIG_OF))
		kfree(heaps);
	return 0;

err_heap:
	for (; i > 0; i--)
		ion_heap_destroy(info->heaps[i]);
	ion_device_destroy(info->idev);
err_alloc:
	if (IS_ENABLED(CONFIG_OF))
		kfree(heaps);
	return err;
}

static int pxa_ion_remove(struct platform_device *pdev)
{
	struct pxa_ion_info *info = platform_get_drvdata(pdev);
	int i;

	if (info) {
		for (i = 0; i < info->heap_cnt; i++)
			ion_heap_destroy(info->heaps[i]);
		ion_device_destroy(info->idev);
		platform_set_drvdata(pdev, NULL);
		if (page_pad)
			__free_page(page_pad);
	}
	return 0;
}

static struct platform_driver pxa_ion_driver = {
	.probe = pxa_ion_probe,
	.remove = pxa_ion_remove,
	.driver = {
		.name = "pxa-ion",
		.of_match_table = of_match_ptr(pxa_ion_dt_match),
	},
};

static int __init pxa_ion_init(void)
{
	return platform_driver_register(&pxa_ion_driver);
}

rootfs_initcall(pxa_ion_init);

MODULE_LICENSE("GPL v2");
