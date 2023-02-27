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
#include <linux/clk-provider.h>

int sprd_iommu_mm_enable(struct sprd_iommu_dev *dev);
int sprd_iommu_mm_disable(struct sprd_iommu_dev *dev);

static void sprd_iommu_mm_prepare(struct sprd_iommu_dev *dev)
{
#ifdef CONFIG_OF
	printk("%s, axi clock:%p\n",__FUNCTION__, dev->mmu_axiclock);
	pr_debug("%s %d, axi_clk prepare_cnt:%d, mm_clk prepare_cnt:%d, enable_cnt:%d\n",__FUNCTION__, __LINE__, __clk_get_prepare_count(dev->mmu_axiclock),
		__clk_get_prepare_count(dev->mmu_mclock), __clk_get_enable_count(dev->mmu_mclock));
	if (dev->mmu_axiclock)
		clk_prepare(dev->mmu_axiclock);
	pr_debug("%s %d, axi_clk prepare_cnt:%d, mm_clk prepare_cnt:%d, enable_cnt:%d\n",__FUNCTION__, __LINE__, __clk_get_prepare_count(dev->mmu_axiclock),
		__clk_get_prepare_count(dev->mmu_mclock), __clk_get_enable_count(dev->mmu_mclock));
	return;
#endif
}

static void sprd_iommu_mm_unprepare(struct sprd_iommu_dev *dev)
{
#ifdef CONFIG_OF
	printk("%s, axi clock:%p\n",__FUNCTION__, dev->mmu_axiclock);
	if (dev->mmu_axiclock)
		clk_unprepare(dev->mmu_axiclock);
	return;
#endif
}

void sprd_iommu_mm_open(struct sprd_iommu_dev *dev)
{
#ifdef CONFIG_ARCH_SCX35L
	printk("%s, light_sleep_en:%d, ddr frq:%d\n",__FUNCTION__, dev->light_sleep_en, emc_clk_get());
#else
	printk("%s, light_sleep_en:%d\n",__FUNCTION__, dev->light_sleep_en);
#endif

        mutex_lock(&dev->mutex_pgt);
        mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
        memset((void *)(dev->init_data->pgt_base),0xFF,PAGE_ALIGN(dev->init_data->pgt_size));
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(1),MMU_RAMCLK_DIV2_EN_MASK);
        mmu_reg_write(dev->init_data->ctrl_reg,dev->init_data->iova_base,MMU_START_MB_ADDR_MASK);
        mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
        mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(1),MMU_EN_MASK);
        mutex_unlock(&dev->mutex_pgt);

	return;
}

void sprd_iommu_mm_release(struct sprd_iommu_dev *dev)
{
	printk("%s\n",__FUNCTION__);

	mutex_lock(&dev->mutex_pgt);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(0),MMU_RAMCLK_DIV2_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
	mutex_unlock(&dev->mutex_pgt);

	return;
}

int sprd_iommu_mm_init(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data)
{
	int err = -1;
#ifdef CONFIG_OF && CONFIG_COMMON_CLK
	struct device_node *np;

	np = dev->misc_dev.this_device->of_node;
	if(!np) {
		return -1;
	}

	dev->mmu_clock = of_clk_get(np, 0) ;
	dev->mmu_mclock = of_clk_get(np, 1);
	dev->mmu_axiclock = of_clk_get(np, 2);
	if (IS_ERR(dev->mmu_axiclock)) {
	        printk ("%s, can't get mm axi clock:%p\n", __FUNCTION__, dev->mmu_axiclock);
	} else {
                dev->light_sleep_en = true;
	}
#else
	dev->mmu_mclock = clk_get(NULL,"clk_mm_i");
	dev->mmu_clock = clk_get(NULL,"clk_mmu");
#endif
	if (IS_ERR(dev->mmu_clock) || IS_ERR(dev->mmu_mclock)) {
		printk ("%s, can't get clock:%p, %p\n", __FUNCTION__, dev->mmu_clock, dev->mmu_mclock);
		goto errorout;
	}

	sprd_iommu_mm_enable(dev);
	err = sprd_iommu_init(dev,data);
	sprd_iommu_mm_disable(dev);
	return err;

errorout:
    if (dev->mmu_clock) {
        clk_put(dev->mmu_clock);
    }

    if (dev->mmu_mclock) {
        clk_put(dev->mmu_mclock);
    }

    if (dev->mmu_axiclock) {
        clk_put(dev->mmu_axiclock);
    }

    return -1;
}

int sprd_iommu_mm_exit(struct sprd_iommu_dev *dev)
{
	int err = -1;
	sprd_iommu_mm_enable(dev);
	err = sprd_iommu_exit(dev);
	sprd_iommu_mm_disable(dev);
	return err;
}

unsigned long sprd_iommu_mm_iova_alloc(struct sprd_iommu_dev *dev, size_t iova_length)
{
	return sprd_iommu_iova_alloc(dev, iova_length);
}

void sprd_iommu_mm_iova_free(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length)
{
	sprd_iommu_iova_free(dev, iova, iova_length);
	return;
}

int sprd_iommu_mm_iova_map(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	int err = -1;

	if (dev->light_sleep_en) {
		mutex_lock(&dev->mutex_map);
		if (0 == dev->map_count)
			sprd_iommu_mm_prepare(dev);

		sprd_iommu_mm_enable(dev);

		if (0 == dev->map_count)
			sprd_iommu_mm_open(dev);
		dev->map_count++;

		err = sprd_iommu_iova_map(dev,iova,iova_length,handle);
		sprd_iommu_mm_disable(dev);
		mutex_unlock(&dev->mutex_map);
	} else {
		mutex_lock(&dev->mutex_map);
		if (0 == dev->map_count)
			sprd_iommu_mm_enable(dev);

		dev->map_count++;

		err = sprd_iommu_iova_map(dev,iova,iova_length,handle);
		mutex_unlock(&dev->mutex_map);
	}
	return err;
}

int sprd_iommu_mm_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	int err = -1;

	if (dev->light_sleep_en) {
		mutex_lock(&dev->mutex_map);
		sprd_iommu_mm_enable(dev);
		err = sprd_iommu_iova_unmap(dev,iova,iova_length,handle);

		dev->map_count--;
		if (0 == dev->map_count)
			sprd_iommu_mm_release(dev);

		sprd_iommu_mm_disable(dev);

		if (0 == dev->map_count)
			sprd_iommu_mm_unprepare(dev);
		mutex_unlock(&dev->mutex_map);
	} else {
		mutex_lock(&dev->mutex_map);
		err = sprd_iommu_iova_unmap(dev,iova,iova_length,handle);

		dev->map_count--;
		if (0 == dev->map_count)
		        sprd_iommu_mm_disable(dev);
		mutex_unlock(&dev->mutex_map);
	}
	return err;
}

int sprd_iommu_mm_backup(struct sprd_iommu_dev *dev)
{
	int err = 0;

	mutex_lock(&dev->mutex_map);
	printk("%s, map_count:%d\n",__FUNCTION__,dev->map_count);

	if (dev->light_sleep_en) {
		if (dev->map_count > 0) {
			sprd_iommu_mm_enable(dev);
			err = sprd_iommu_backup(dev);
			sprd_iommu_mm_disable(dev);
			sprd_iommu_mm_unprepare(dev);
		}
	} else {
		if (dev->map_count > 0)
			err=sprd_iommu_backup(dev);
	}
	mutex_unlock(&dev->mutex_map);
	return err;
}

int sprd_iommu_mm_restore(struct sprd_iommu_dev *dev)
{
	int err = 0;

	mutex_lock(&dev->mutex_map);
	printk("%s, map_count:%d\n",__FUNCTION__,dev->map_count);

	if (dev->light_sleep_en) {
		if (dev->map_count > 0) {
			sprd_iommu_mm_prepare(dev);
			sprd_iommu_mm_enable(dev);
			err = sprd_iommu_restore(dev);
			sprd_iommu_mm_disable(dev);
		}
	} else {
		if (dev->map_count > 0)
			err=sprd_iommu_restore(dev);
	}
	mutex_unlock(&dev->mutex_map);
	return err;
}

int sprd_iommu_mm_disable(struct sprd_iommu_dev *dev)
{
	static int cnt = 0;
	int i, taint_flag = 0;
	int *p;


	sprd_iommu_disable(dev);

	if (!dev->light_sleep_en) {
		printk("%s\n",__FUNCTION__);
		mutex_lock(&dev->mutex_pgt);
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(0),MMU_RAMCLK_DIV2_EN_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
		mutex_unlock(&dev->mutex_pgt);
	}
#ifdef CONFIG_OF
	clk_disable_unprepare(dev->mmu_clock);
	if (dev->mmu_mclock)
		clk_disable_unprepare(dev->mmu_mclock);
#else
	clk_disable(dev->mmu_clock);
	clk_disable(dev->mmu_mclock);
#endif

	//let's check fault_page whether it's is tainted or not
	for(i = 0, p = dev->fault_page; i < PAGE_SIZE/sizeof(unsigned long); i++) {
		if ( p[i] != 0xDEADDEAD )
		{
			p[i] = 0xDEADDEAD;
			if(!taint_flag)
				cnt++, taint_flag = 1;
		}
	}
	WARN(taint_flag,"%s: fault_page has been tainted by wrong IOMMU operation, cnt is %d\n", __func__, cnt);

	return 0;
}

int sprd_iommu_mm_enable(struct sprd_iommu_dev *dev)
{
#ifdef CONFIG_OF
	if (dev->mmu_mclock)
		clk_prepare_enable(dev->mmu_mclock);
	clk_prepare_enable(dev->mmu_clock);
#else
	clk_enable(dev->mmu_mclock);
	clk_enable(dev->mmu_clock);
	udelay(100);
#endif

	sprd_iommu_enable(dev);

	if (!dev->light_sleep_en) {
#ifdef CONFIG_ARCH_SCX35L
		printk("%s, ddr frq:%d\n",__FUNCTION__,emc_clk_get());
#else
		printk("%s\n",__FUNCTION__);
#endif
	mutex_lock(&dev->mutex_pgt);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
#if 0
	memset((void *)(dev->init_data->pgt_base),0xFF,PAGE_ALIGN(dev->init_data->pgt_size));
#else
	{
		int i;
		unsigned long *pa_fault_page = (__pa(dev->fault_page) | 0xFFF);
		for(i = 0; i < PAGE_ALIGN(dev->init_data->pgt_size)/sizeof(unsigned long); i++)
			((unsigned long *)dev->init_data->pgt_base)[i] = pa_fault_page;
	}
#endif
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(1),MMU_RAMCLK_DIV2_EN_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,dev->init_data->iova_base,MMU_START_MB_ADDR_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(1),MMU_EN_MASK);
		mutex_unlock(&dev->mutex_pgt);
	}
	return 0;
}

int sprd_iommu_mm_dump(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length)
{
	return sprd_iommu_dump(dev, iova, iova_length);
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
	.open=sprd_iommu_mm_open,
	.release=sprd_iommu_mm_release,
};

