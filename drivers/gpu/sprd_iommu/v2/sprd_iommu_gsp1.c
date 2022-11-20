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

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sprd_iommu_gsp1_early_suspend(struct early_suspend *es)
{
	int err = -1;

	struct sprd_iommu_dev *dev = container_of(es,
					struct sprd_iommu_dev, early_suspend);

	mutex_lock(&dev->mutex_map);
	pr_info("%s, map_count:%d\n", __func__, dev->map_count);
	if (dev->map_count > 0)
		err = sprd_iommu_backup(dev);
	mutex_unlock(&dev->mutex_map);
}

static void sprd_iommu_gsp1_late_resume(struct early_suspend *es)
{
	int err = -1;

	struct sprd_iommu_dev *dev = container_of(es,
					struct sprd_iommu_dev, early_suspend);

	mutex_lock(&dev->mutex_map);
	pr_info("%s, map_count:%d\n", __func__, dev->map_count);
	if (dev->map_count > 0)
		err = sprd_iommu_restore(dev);
	mutex_unlock(&dev->mutex_map);
}
#endif

void sprd_iommu_gsp1_clk_enable(struct sprd_iommu_dev *dev)
{
	pr_debug("%s\n", __func__);
#ifndef CONFIG_SC_FPGA
	clk_prepare_enable(dev->mmu_mclock);
	clk_prepare_enable(dev->mmu_clock);
#endif
}

void sprd_iommu_gsp1_clk_disable(struct sprd_iommu_dev *dev)
{
	pr_debug("%s\n", __func__);
#ifndef CONFIG_SC_FPGA
	clk_disable_unprepare(dev->mmu_clock);
	clk_disable_unprepare(dev->mmu_mclock);
#endif
}

void sprd_iommu_gsp1_open(struct sprd_iommu_dev *dev)
{
}

void sprd_iommu_gsp1_release(struct sprd_iommu_dev *dev)
{
}

int sprd_iommu_gsp1_init(struct sprd_iommu_dev *dev,
			struct sprd_iommu_init_data *data)
{
	int err = -1;
#ifndef CONFIG_SC_FPGA
	struct device_node *np;

	np = dev->misc_dev.this_device->of_node;
	if (!np)
		return -1;

	dev->mmu_mclock = of_clk_get(np, 0);
	dev->mmu_clock = of_clk_get(np, 2);

	if (IS_ERR(dev->mmu_clock) || IS_ERR(dev->mmu_mclock)) {
		pr_info("%s, can't get clock:%p, %p\n", __func__,
			dev->mmu_clock, dev->mmu_mclock);
		goto errorout;
	}
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.suspend = sprd_iommu_gsp1_early_suspend;
	dev->early_suspend.resume  = sprd_iommu_gsp1_late_resume;
	dev->early_suspend.level   = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	register_early_suspend(&dev->early_suspend);
#endif

	sprd_iommu_gsp1_clk_enable(dev);
	err = sprd_iommu_init(dev, data);
	sprd_iommu_gsp1_clk_disable(dev);

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

int sprd_iommu_gsp1_exit(struct sprd_iommu_dev *dev)
{
	int err = -1;

	sprd_iommu_gsp1_clk_enable(dev);
	err = sprd_iommu_exit(dev);
	sprd_iommu_gsp1_clk_disable(dev);

	return err;
}

unsigned long sprd_iommu_gsp1_iova_alloc(struct sprd_iommu_dev *dev,
					size_t iova_length)
{
	return sprd_iommu_iova_alloc(dev, iova_length);
}

void sprd_iommu_gsp1_iova_free(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length)
{
	sprd_iommu_iova_free(dev, iova, iova_length);
}

int sprd_iommu_gsp1_iova_map(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length, struct sg_table *table)
{
	int err = -1;

	mutex_lock(&dev->mutex_map);
	if (0 == dev->map_count)
		sprd_iommu_enable(dev);
	dev->map_count++;

	err = sprd_iommu_iova_map(dev, iova, iova_length, table);
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_gsp1_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova,
				size_t iova_length)
{
	int err = -1;

	mutex_lock(&dev->mutex_map);
	err = sprd_iommu_iova_unmap(dev, iova, iova_length);

	dev->map_count--;
	if (0 == dev->map_count)
		sprd_iommu_disable(dev);
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_gsp1_backup(struct sprd_iommu_dev *dev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	return 0;
#else
	int err = -1;

	mutex_lock(&dev->mutex_map);
	pr_info("%s, map_count:%d\n", __func__, dev->map_count);
	if (dev->map_count > 0)
		err = sprd_iommu_backup(dev);
	mutex_unlock(&dev->mutex_map);

	return err;
#endif
}

int sprd_iommu_gsp1_restore(struct sprd_iommu_dev *dev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	return 0;
#else
	int err = -1;

	mutex_lock(&dev->mutex_map);
	pr_info("%s, map_count:%d\n", __func__, dev->map_count);
	if (dev->map_count > 0)
		err = sprd_iommu_restore(dev);
	mutex_unlock(&dev->mutex_map);

	return 0;
#endif
}

int sprd_iommu_gsp1_dump(struct sprd_iommu_dev *dev, unsigned long iova,
			size_t iova_length)
{
	return sprd_iommu_dump(dev, iova, iova_length);
}

struct sprd_iommu_ops iommu_gsp1_ops = {
	.init = sprd_iommu_gsp1_init,
	.exit = sprd_iommu_gsp1_exit,
	.iova_alloc = sprd_iommu_gsp1_iova_alloc,
	.iova_free = sprd_iommu_gsp1_iova_free,
	.iova_map = sprd_iommu_gsp1_iova_map,
	.iova_unmap = sprd_iommu_gsp1_iova_unmap,
	.backup = sprd_iommu_gsp1_backup,
	.restore = sprd_iommu_gsp1_restore,
	.enable = sprd_iommu_gsp1_clk_enable,
	.disable = sprd_iommu_gsp1_clk_disable,
	.dump = sprd_iommu_gsp1_dump,
	.open = sprd_iommu_gsp1_open,
	.release = sprd_iommu_gsp1_release,
};

