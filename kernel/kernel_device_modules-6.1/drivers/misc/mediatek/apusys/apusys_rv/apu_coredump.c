// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#include <apu.h>
#include <apu_excep.h>


void apu_setup_dump(struct mtk_apu *apu, dma_addr_t da)
{
	/* Set dump addr in mbox */
	apu->conf_buf->ramdump_offset = da;

	/* Set coredump type(AP dump by default) */
	apu->conf_buf->ramdump_type = 0;
}

int apu_coredump_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int ret = 0;
	void *domain;
	uint32_t coredump_size = apu->up_code_buf_sz + REG_SIZE +
		TBUF_SIZE + CACHE_DUMP_SIZE;

	if (apu->platdata->flags & F_SECURE_COREDUMP)
		return 0;

	apu->coredump = kzalloc(sizeof(struct apu_coredump), GFP_KERNEL);
	if (!apu->coredump)
		return -ENOMEM;

	apu->coredump_buf = dma_alloc_coherent(
		apu->dev, coredump_size,
			&apu->coredump_da, GFP_KERNEL);
	if (apu->coredump_buf == NULL || apu->coredump_da == 0) {
		dev_info(dev, "%s: dma_alloc_coherent fail\n", __func__);
		return -ENOMEM;
	}

	if ((apu->platdata->flags & F_BYPASS_IOMMU) == 0) {
		domain = iommu_get_domain_for_dev(apu->dev);
		if (domain == NULL) {
			dev_info(dev, "%s: iommu_get_domain_for_dev fail\n", __func__);
			return -ENOMEM;
		}
		apu->coredump_buf_pa = iommu_iova_to_phys(domain,
			apu->coredump_da);
	} else {
		apu->coredump_buf_pa = apu->coredump_da;
	}

	apu->coredump->tcmdump = (char *) apu->coredump_buf;
	apu->coredump->ramdump = (char *) ((void *)apu->coredump->tcmdump + apu->md32_tcm_sz);
	apu->coredump->regdump = (char *) ((void *)apu->coredump->tcmdump + apu->up_code_buf_sz);
	apu->coredump->tbufdump = (char *) ((void *)apu->coredump->regdump + REG_SIZE);
	apu->coredump->cachedump = (uint32_t *) ((void *)apu->coredump->tbufdump + TBUF_SIZE);

	dev_info(dev, "%s: apu->coredump_buf = 0x%llx, apu->coredump_da = 0x%llx\n",
		__func__, (uint64_t) apu->coredump_buf,
		(uint64_t) apu->coredump_da);

	dev_info(dev, "%s: apu->coredump_buf_pa = 0x%llx\n",
		__func__, apu->coredump_buf_pa);

	memset(apu->coredump_buf, 0, coredump_size);

	apu_setup_dump(apu, apu->coredump_da);

	return ret;
}

void apu_coredump_remove(struct mtk_apu *apu)
{
	uint32_t coredump_size = apu->up_code_buf_sz + REG_SIZE +
		TBUF_SIZE + CACHE_DUMP_SIZE;

	if ((apu->platdata->flags & F_SECURE_COREDUMP) == 0) {
		dma_free_coherent(
			apu->dev, coredump_size, apu->coredump_buf, apu->coredump_da);
		kfree(apu->coredump);
	}
}
