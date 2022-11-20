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
#ifdef CONFIG_OF && CONFIG_COMMON_CLK
	struct device_node *np;

	np = dev->misc_dev.this_device->of_node;
	if(!np) {
		return -1;
	}

	dev->mmu_clock=of_clk_get(np, 0) ;
	dev->mmu_mclock=of_clk_get(np,1);
#else
	dev->mmu_mclock= clk_get(NULL,"clk_mm_i");
	dev->mmu_clock=clk_get(NULL,"clk_mmu");
	if (!dev->mmu_mclock) {
		printk ("%s, cant get dev->mmu_mclock\n", __FUNCTION__);
		return -1;
	}
#endif
	if (!dev->mmu_clock) {
		printk ("%s, cant get dev->mmu_clock\n", __FUNCTION__);
		return -1;
	}

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

	mutex_lock(&dev->mutex_map);
	if (0 == dev->map_count)
		sprd_iommu_mm_enable(dev);
	dev->map_count++;

	err = sprd_iommu_iova_map(dev,iova,iova_length,handle);
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_mm_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	int err=-1;

	mutex_lock(&dev->mutex_map);
	err = sprd_iommu_iova_unmap(dev,iova,iova_length,handle);

	dev->map_count--;
	if (0 == dev->map_count)
		sprd_iommu_mm_disable(dev);
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_mm_backup(struct sprd_iommu_dev *dev)
{
	int err=0;

	if (dev->map_count > 0)
		err=sprd_iommu_backup(dev);

	return err;
}

int sprd_iommu_mm_restore(struct sprd_iommu_dev *dev)
{
	int err=0;

	if (dev->map_count > 0)
		err=sprd_iommu_restore(dev);

	return err;
}

int sprd_iommu_mm_disable(struct sprd_iommu_dev *dev)
{
	printk("%s line:%d\n",__FUNCTION__,__LINE__);

	sprd_iommu_disable(dev);

	mutex_lock(&dev->mutex_pgt);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(0),MMU_RAMCLK_DIV2_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
	mutex_unlock(&dev->mutex_pgt);

#ifdef CONFIG_OF
	clk_disable_unprepare(dev->mmu_clock);
	if (dev->mmu_mclock)
		clk_disable_unprepare(dev->mmu_mclock);
#else
	clk_disable(dev->mmu_clock);
	clk_disable(dev->mmu_mclock);
#endif
	return 0;
}

int sprd_iommu_mm_enable(struct sprd_iommu_dev *dev)
{
	printk("%s line:%d\n",__FUNCTION__,__LINE__);

#ifdef CONFIG_OF
	if (dev->mmu_mclock)
		clk_prepare_enable(dev->mmu_mclock);
	clk_prepare_enable(dev->mmu_clock);
#else
	clk_enable(dev->mmu_mclock);
	clk_enable(dev->mmu_clock);
#endif
	udelay(100);

	sprd_iommu_enable(dev);

	mutex_lock(&dev->mutex_pgt);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
	memset((unsigned long *)(dev->init_data->pgt_base),0xFFFFFFFF,PAGE_ALIGN(dev->init_data->pgt_size));
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(1),MMU_RAMCLK_DIV2_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,dev->init_data->iova_base,MMU_START_MB_ADDR_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(1),MMU_EN_MASK);
	mutex_unlock(&dev->mutex_pgt);

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

