// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#define pr_fmt(fmt)    "iommu_engine: test " fmt

#include "iommu_engine.h"

static struct dma_engine_data *dma_engine_datas[DMA_ENGINE_NUM];

#define TOP_ADDRESS(addr) ((unsigned int)((addr >> 32) & 0xf))
#define LOW_ADDRESS(addr) ((unsigned int)(addr & 0xffffffff))

static void dump_char(char *str, unsigned char *pa)
{
	pr_info("%s: pa=0x%llx, data=0x%llx\n",
		str, (unsigned long long)pa, (unsigned long long)(*pa));
}

static void cqdma_set(unsigned long long src, unsigned long long dst,
		      unsigned int sz)
{
	void __iomem *cqdma_reg_base = dma_engine_datas[DMA_ENGINE_CQDMA]->reg_base;

	pr_info("%s, src=0x%llx, dst=0x%llx, sz=%d\n", __func__, src, dst, sz);

	/* src */
	writel_relaxed(TOP_ADDRESS(src), cqdma_reg_base + 0x60);
	writel_relaxed(LOW_ADDRESS(src), cqdma_reg_base + 0x1c);
	/* dst */
	writel_relaxed(TOP_ADDRESS(dst), cqdma_reg_base + 0x64);
	writel_relaxed(LOW_ADDRESS(dst), cqdma_reg_base + 0x20);
	/* len */
	writel_relaxed(sz, cqdma_reg_base + 0x24);
	/* trigger */
	writel_relaxed(0x1, cqdma_reg_base + 0x8);
}

static void cqdma_access(unsigned long long dma0addr, void *va00,
			 unsigned long long dma1addr,
			 void *va11, unsigned int sz)
{
	unsigned char *va0 = (unsigned char *)va00;
	unsigned char *va1 = (unsigned char *)va11;
	int i;

	for (i = 0; i < SZ_4K; i++) {
		va0[i] = (unsigned char)i;
		va1[i] = 0;
	}

	/* before cqdma */
	pr_info("---before---\n");
	for (i = 0; i < CQDMA_BUFLEN; i++) {
		dump_char("src", &va0[i]);
		dump_char("dst", &va1[i]);
	}

	/* cqdma access*/
	cqdma_set(dma0addr, dma1addr, sz);

	pr_info("---after---\n");
	for (i = 0; i < CQDMA_BUFLEN; i++) {
		dump_char("src", &va0[i]);
		dump_char("dst", &va1[i]);
	}
}

/**
 * Translation Fault handler
 */
static int default_iommu_fault_handler(struct iommu_fault *fault, void *cookie)
{
	struct device *dev;
	int ret = 0;

	if (!fault || !cookie)
		return -EINVAL;

	dev = cookie;

	dev_info(dev, "[%s] dev:%s, start\n", __func__, dev_name(dev));

	if (fault->type == IOMMU_FAULT_PAGE_REQ) {
		dev_info(dev,
			 "[%s][page request fault] type:%d, prm{flags:0x%x, grpid:0x%x, perm:0x%x, addr:0x%llx, pasid:0x%x}\n",
			 __func__, fault->type, fault->prm.flags,
			 fault->prm.grpid, fault->prm.perm,
			 fault->prm.addr, fault->prm.pasid);
	} else if (fault->type == IOMMU_FAULT_DMA_UNRECOV) {
		dev_info(dev,
			 "[%s][unrecoverable fault] type:%d, event{reason:0x%x, flags:0x%x, perm:0x%x, addr:0x%llx, pasid:0x%x}\n",
			 __func__, fault->type, fault->event.reason,
			 fault->event.flags, fault->event.perm,
			 fault->event.addr, fault->event.pasid);
	}

	dev_info(dev, "[%s] dev:%s, done\n", __func__, dev_name(dev));

	return ret;
}

static int cqdma_engine_init(struct platform_device *pdev)
{
	struct dma_engine_data *data;
	void __iomem *cqdma_reg_base;
	struct resource *res;

	pr_info("%s start dev:%s\n", __func__, dev_name(&pdev->dev));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cqdma_reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(cqdma_reg_base))
		return PTR_ERR(cqdma_reg_base);

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->engine = DMA_ENGINE_CQDMA;
	data->pdev = pdev;
	data->reg_base = cqdma_reg_base;
	data->iommu_fault_handler = default_iommu_fault_handler;

	dma_engine_datas[DMA_ENGINE_CQDMA] = data;

	/* Register Translation Fault handler */
	iommu_register_device_fault_handler(&pdev->dev,
		(iommu_dev_fault_handler_t)data->iommu_fault_handler, &pdev->dev);

	pr_info("%s done dev:%s, cqdma_reg_base:%llx\n", __func__,
		dev_name(&pdev->dev), (unsigned long long)cqdma_reg_base);

	return 0;
}

int engine_access(int engine, struct device *dev, struct dma_device_res *res)
{
	switch (engine) {
	case DMA_ENGINE_CQDMA:
		pr_info("%s cqdma_access start, dev:%s\n", __func__, dev_name(dev));
		cqdma_access(res[0].dma_addr, res[0].vaddr,
			     res[1].dma_addr, res[1].vaddr,
			     CQDMA_BUFLEN);
		pr_info("%s cqdma_access done, dev:%s\n", __func__, dev_name(dev));
		break;
	default:
		pr_info("%s not support engine:%d\n", __func__, engine);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(engine_access);

int engine_init(int engine, struct platform_device *pdev)
{
	int ret;

	switch (engine) {
	case DMA_ENGINE_CQDMA:
		ret = cqdma_engine_init(pdev);
		break;
	default:
		pr_info("%s not support engine:%d\n", __func__, engine);
		return -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(engine_init);

MODULE_DESCRIPTION("MediaTek IOMMU test engine");
MODULE_LICENSE("GPL");
