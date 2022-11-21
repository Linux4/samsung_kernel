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

void sprd_iommu_vpp_clk_enable(struct sprd_iommu_dev *dev)
{
	pr_info("%s\n", __func__);
#ifndef CONFIG_SC_FPGA
	clk_prepare_enable(dev->mmu_mclock);
	clk_prepare_enable(dev->mmu_clock);
#endif
}

void sprd_iommu_vpp_clk_disable(struct sprd_iommu_dev *dev)
{
	pr_info("%s\n", __func__);
#ifndef CONFIG_SC_FPGA
	clk_disable_unprepare(dev->mmu_clock);
	clk_disable_unprepare(dev->mmu_mclock);
#endif
}

void sprd_iommu_vpp_open(struct sprd_iommu_dev *dev)
{
}

void sprd_iommu_vpp_release(struct sprd_iommu_dev *dev)
{
}

int sprd_iommu_vpp_init(struct sprd_iommu_dev *dev,
			struct sprd_iommu_init_data *data)
{
	int err = -1;
#ifndef CONFIG_SC_FPGA
	struct device_node *np;

	np = dev->misc_dev.this_device->of_node;
	if (!np)
		return -1;

	dev->mmu_clock = of_clk_get(np, 0);
	dev->mmu_mclock = of_clk_get(np, 1);

	if (IS_ERR(dev->mmu_clock) || IS_ERR(dev->mmu_mclock)) {
		pr_info("%s, can't get clock:%p, %p\n", __func__,
			dev->mmu_clock, dev->mmu_mclock);
		goto errorout;
	}
#endif
	sprd_iommu_vpp_clk_enable(dev);
	err = sprd_iommu_init(dev, data);
	sprd_iommu_vpp_clk_disable(dev);

	return err;

#ifndef CONFIG_SC_FPGA
errorout:
	if (dev->mmu_clock)
		clk_put(dev->mmu_clock);

	if (dev->mmu_mclock)
		clk_put(dev->mmu_mclock);

	return -1;
#endif
}

int sprd_iommu_vpp_exit(struct sprd_iommu_dev *dev)
{
	int err = -1;

	sprd_iommu_vpp_clk_enable(dev);
	err = sprd_iommu_exit(dev);
	sprd_iommu_vpp_clk_disable(dev);

	return err;
}

unsigned long sprd_iommu_vpp_iova_alloc(struct sprd_iommu_dev *dev,
					size_t iova_length)
{
	return sprd_iommu_iova_alloc(dev, iova_length);
}

void sprd_iommu_vpp_iova_free(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length)
{
	return sprd_iommu_iova_free(dev, iova, iova_length);
}

int sprd_iommu_vpp_iova_map(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length, struct sg_table *table)
{
	int err = -1;

	mutex_lock(&dev->mutex_map);
	if (0 == dev->map_count) {
		sprd_iommu_vpp_clk_enable(dev);
		sprd_iommu_reset_enable(dev);
	}
	dev->map_count++;
	err = sprd_iommu_iova_map(dev, iova, iova_length, table);
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_vpp_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length)
{
	int err = -1;

	mutex_lock(&dev->mutex_map);
	err = sprd_iommu_iova_unmap(dev, iova, iova_length);
	dev->map_count--;
	if (0 == dev->map_count) {
		sprd_iommu_disable(dev);
		sprd_iommu_vpp_clk_disable(dev);
	}
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_vpp_backup(struct sprd_iommu_dev *dev)
{
	int err = 0;

	mutex_lock(&dev->mutex_map);
	pr_info("%s, map_count:%d\n", __func__, dev->map_count);

	if (dev->map_count > 0) {
		err = sprd_iommu_backup(dev);
		sprd_iommu_vpp_clk_disable(dev);
	}
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_vpp_restore(struct sprd_iommu_dev *dev)
{
	int err = 0;

	mutex_lock(&dev->mutex_map);
	pr_info("%s, map_count:%d\n", __func__, dev->map_count);

	if (dev->map_count > 0) {
		sprd_iommu_vpp_clk_enable(dev);
		err = sprd_iommu_restore(dev);
	}
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_vpp_dump(struct sprd_iommu_dev *dev, unsigned long iova,
			size_t iova_length)
{
	return sprd_iommu_dump(dev, iova, iova_length);
}

struct sprd_iommu_ops iommu_vpp_ops = {
	.init = sprd_iommu_vpp_init,
	.exit = sprd_iommu_vpp_exit,
	.iova_alloc = sprd_iommu_vpp_iova_alloc,
	.iova_free = sprd_iommu_vpp_iova_free,
	.iova_map = sprd_iommu_vpp_iova_map,
	.iova_unmap = sprd_iommu_vpp_iova_unmap,
	.backup = sprd_iommu_vpp_backup,
	.restore = sprd_iommu_vpp_restore,
	.enable = sprd_iommu_vpp_clk_enable,
	.disable = sprd_iommu_vpp_clk_disable,
	.dump = sprd_iommu_vpp_dump,
	.open = sprd_iommu_vpp_open,
	.release = sprd_iommu_vpp_release,
};

