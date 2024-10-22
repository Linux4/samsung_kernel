// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/iommu.h>
#include <linux/of_address.h>

#define IOVA_START 0x40000000
#define IOVA_SIZE 0x200000000
#define ENABLE_DEBUG_SMMU 0
static int gpu_iommu_probe(struct platform_device *pdev)
{
#if ENABLE_DEBUG_SMMU
	struct device *dev = &pdev->dev;
	int ret;
	struct iommu_domain *domain;
	unsigned long iova, size;
	phys_addr_t phys;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		dev_err(dev, "no IOMMU domain found for gpu iommu\n");
		return -EINVAL;
	}

	phys = IOVA_START;
	size = IOVA_SIZE;
	iova = phys;	/* We just want a direct mapping */

	dev_vdbg(dev, "[gpu_iommu] %s iommu_map %lx -> %llx (%lx), name %s\n", __func__,
		iova, phys, size, pdev->name);

	ret = iommu_map(domain, iova, phys, size, IOMMU_READ | IOMMU_WRITE);
	if (ret) {
		dev_err(dev, "iommu map fail\n");
		return ret;
	}
#else
	pr_info("[gpu_iommu] %s\n", __func__);
#endif
	return 0;
}

static int gpu_iommu_remove(struct platform_device *pdev)
{
	pr_info("[gpu_iommu] %s\n", __func__);
	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "mediatek,gpu-iommu" },
	{ /* end of table */ }
};

static struct platform_driver gpu_iommu_driver = {
	.probe = gpu_iommu_probe,
	.remove = gpu_iommu_remove,
	.driver = {
		.name = "gpu_iommu_mt6989",
		.of_match_table = match_table,
	},
};
module_platform_driver(gpu_iommu_driver);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek GPU IOMMU Driver");
MODULE_LICENSE("GPL");
