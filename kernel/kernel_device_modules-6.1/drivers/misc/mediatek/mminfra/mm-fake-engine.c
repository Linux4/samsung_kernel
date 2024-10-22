// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

static void mm_config_dma_mask(struct device *dev)
{
	u32 dma_mask_bit = 0;
	s32 ret;

	ret = of_property_read_u32(dev->of_node, "dma-mask-bit",
		&dma_mask_bit);

	if (ret != 0 || !dma_mask_bit)
		return;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(dma_mask_bit));
	pr_notice("%s set dma mask bit:%u result:%d\n", __func__,
		dma_mask_bit, ret);

	if (dev->dma_parms) {
		ret = dma_set_max_seg_size(dev, UINT_MAX);
		if (ret)
			pr_notice("%s Failed to set DMA segment size\n", __func__);
	}
}

static int mm_fake_eng_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_notice("%s for smmu fake dev", __func__);
	mm_config_dma_mask(dev);

	return 0;
}

static int mm_fake_eng_remove(struct platform_device *pdev)
{
	pr_notice("%s for smmu fake dev", __func__);
	return 0;
}

static const struct of_device_id mm_fake_eng_of_ids[] = {
	{.compatible = "mediatek,smmu-share-group"},
	{}
};
MODULE_DEVICE_TABLE(of, mm_fake_eng_of_ids);

static struct platform_driver mm_fake_eng_drv = {
	.probe = mm_fake_eng_probe,
	.remove = mm_fake_eng_remove,
	.driver = {
		.name = "mm_fake_eng",
		.of_match_table = mm_fake_eng_of_ids,
	},
};
module_platform_driver(mm_fake_eng_drv);

MODULE_LICENSE("GPL");

