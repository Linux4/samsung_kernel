/*
 * drivers/gpu/iommu/iommu.c *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include"../sprd_iommu_common.h"

int sprd_iommu_mm_enable(struct sprd_iommu_dev *dev);
int sprd_iommu_mm_disable(struct sprd_iommu_dev *dev);

int sprd_iommu_mm_init(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data)
{
	int err=-1;
	dev->mmu_mclock= clk_get(NULL,"clk_mm_i");
	dev->mmu_clock=clk_get(NULL,"clk_mmu");
	if((NULL==dev->mmu_mclock)||(NULL==dev->mmu_clock))
		return -1;
	sprd_iommu_mm_enable(dev);
	err=sprd_iommu_init(dev,data);
	sprd_iommu_mm_disable(dev);
	return err;
}

int sprd_iommu_mm_exit(struct sprd_iommu_dev *dev)
{
	int err=-1;
	sprd_iommu_mm_enable(dev);
	err=sprd_iommu_exit(dev);
	sprd_iommu_mm_disable(dev);
	return err;
}

unsigned long sprd_iommu_mm_iova_alloc(struct sprd_iommu_dev *dev, size_t iova_length)
{
	return sprd_iommu_iova_alloc(dev,iova_length);
}

void sprd_iommu_mm_iova_free(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length)
{
	sprd_iommu_iova_free(dev,iova,iova_length);
	return;
}

int sprd_iommu_mm_iova_map(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	int err=-1;
	sprd_iommu_mm_enable(dev);
	err = sprd_iommu_iova_map(dev,iova,iova_length,handle);
	sprd_iommu_mm_disable(dev);
	return err;
}

int sprd_iommu_mm_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	int err=-1;
	sprd_iommu_mm_enable(dev);
	err = sprd_iommu_iova_unmap(dev,iova,iova_length,handle);
	sprd_iommu_mm_disable(dev);
	return err;
}

int sprd_iommu_mm_backup(struct sprd_iommu_dev *dev)
{
	int err=-1;
	sprd_iommu_mm_enable(dev);
	err=sprd_iommu_backup(dev);
	sprd_iommu_mm_disable(dev);
	return err;
}

int sprd_iommu_mm_restore(struct sprd_iommu_dev *dev)
{
	int err=-1;
	sprd_iommu_mm_enable(dev);
	err=sprd_iommu_restore(dev);
	sprd_iommu_mm_disable(dev);
	return err;
}

int sprd_iommu_mm_disable(struct sprd_iommu_dev *dev)
{
	sprd_iommu_disable(dev);
	clk_disable(dev->mmu_clock);
	clk_disable(dev->mmu_mclock);
	return 0;
}

int sprd_iommu_mm_enable(struct sprd_iommu_dev *dev)
{
	clk_enable(dev->mmu_mclock);
	clk_enable(dev->mmu_clock);
	udelay(300);
	sprd_iommu_enable(dev);
	return 0;
}

int sprd_iommu_mm_dump(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length)
{
	return sprd_iommu_dump(dev,iova,iova_length);
}

struct sprd_iommu_ops iommu_mm_ops={
	.init=sprd_iommu_mm_init,
	.exit=sprd_iommu_mm_exit,
	.iova_alloc=sprd_iommu_mm_iova_alloc,
	.iova_free=sprd_iommu_mm_iova_free,
	.iova_map=sprd_iommu_mm_iova_map,
	.iova_unmap=sprd_iommu_mm_iova_unmap,
	.backup=sprd_iommu_mm_backup,
	.restore=sprd_iommu_mm_restore,
	.disable=sprd_iommu_mm_disable,
	.enable=sprd_iommu_mm_enable,
	.dump=sprd_iommu_mm_dump,
};

