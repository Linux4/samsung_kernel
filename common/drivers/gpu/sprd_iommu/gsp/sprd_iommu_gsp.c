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

#ifdef GSP_IOMMU_WORKAROUND1
int sprd_iommu_gsp_enable_withworkaround(struct sprd_iommu_dev *dev);
#endif
int sprd_iommu_gsp_enable(struct sprd_iommu_dev *dev);
int sprd_iommu_gsp_disable(struct sprd_iommu_dev *dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sprd_iommu_gsp_early_suspend(struct early_suspend* es);
static void sprd_iommu_gsp_late_resume(struct early_suspend* es);
#endif

void sprd_iommu_gsp_open(struct sprd_iommu_dev *dev)
{
	pr_debug("%s\n",__FUNCTION__);
	mutex_lock(&dev->mutex_pgt);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(1),MMU_RAMCLK_DIV2_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,dev->init_data->iova_base,MMU_START_MB_ADDR_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(1),MMU_EN_MASK);
	mutex_unlock(&dev->mutex_pgt);
	return;
}

void sprd_iommu_gsp_release(struct sprd_iommu_dev *dev)
{
	pr_debug("%s\n",__FUNCTION__);
	mutex_lock(&dev->mutex_pgt);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(0),MMU_RAMCLK_DIV2_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
	mutex_unlock(&dev->mutex_pgt);
	return;
}

int sprd_iommu_gsp_init(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data)
{
		int err=-1;
#if defined(CONFIG_ARCH_SCX30G)
	#ifdef CONFIG_OF
		struct device_node *np;

		np = dev->misc_dev.this_device->of_node;
		if(!np) {
			return -1;
		}
		dev->mmu_mclock=of_clk_get(np, 0) ;
		dev->mmu_clock=of_clk_get(np, 2) ;
	#else
		dev->mmu_mclock= clk_get(NULL,"clk_gsp_emc");
		dev->mmu_clock=clk_get(NULL,"clk_gsp");
	#endif
		if (!dev->mmu_mclock)
			printk ("%s, cant get clk_gsp_emc\n", __FUNCTION__);

		if (!dev->mmu_clock)
			printk ("%s, cant get dev->mmu_clock\n", __FUNCTION__);

		if((NULL==dev->mmu_mclock)||(NULL==dev->mmu_clock))
			return -1;
#elif defined(CONFIG_ARCH_SCX15)
	#ifdef CONFIG_OF
		struct device_node *np;

		np = dev->misc_dev.this_device->of_node;
		if(!np) {
			return -1;
		}
		dev->mmu_mclock= of_clk_get(np, 0) ;
		dev->mmu_pclock= of_clk_get(np, 1) ;
		dev->mmu_clock=of_clk_get(np, 2) ;
	#else
		dev->mmu_mclock= clk_get(NULL,"clk_disp_emc");
		dev->mmu_pclock= clk_get(NULL,"clk_153m6");
		dev->mmu_clock=clk_get(NULL,"clk_gsp");
	#endif
		if (!dev->mmu_mclock)
			printk ("%s, cant get clk_disp_emc\n", __FUNCTION__);
		if (!dev->mmu_pclock)
			printk ("%s, cant get clk_153m6\n", __FUNCTION__);
		if (!dev->mmu_clock)
			printk ("%s, cant get dev->mmu_clock\n", __FUNCTION__);

		if((NULL==dev->mmu_mclock)||(NULL==dev->mmu_pclock)||(NULL==dev->mmu_clock))
			return -1;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.suspend = sprd_iommu_gsp_early_suspend;
	dev->early_suspend.resume  = sprd_iommu_gsp_late_resume;
	dev->early_suspend.level   = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	register_early_suspend(&dev->early_suspend);
#endif

	sprd_iommu_gsp_enable(dev);
	err=sprd_iommu_init(dev,data);
	sprd_iommu_gsp_disable(dev);
	return err;
}

int sprd_iommu_gsp_exit(struct sprd_iommu_dev *dev)
{
	int err=-1;
	sprd_iommu_gsp_enable(dev);
	err=sprd_iommu_exit(dev);
	sprd_iommu_gsp_disable(dev);
	return err;
}

unsigned long sprd_iommu_gsp_iova_alloc(struct sprd_iommu_dev *dev, size_t iova_length)
{
	return sprd_iommu_iova_alloc(dev,iova_length);
}

void sprd_iommu_gsp_iova_free(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length)
{
	sprd_iommu_iova_free(dev,iova,iova_length);
	return;
}

int sprd_iommu_gsp_iova_map(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	int err = -1;

	mutex_lock(&dev->mutex_map);
	if (0 == dev->map_count)
		sprd_iommu_gsp_open(dev);
	dev->map_count++;

	err = sprd_iommu_iova_map(dev,iova,iova_length,handle);
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_gsp_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	int err = -1;

	mutex_lock(&dev->mutex_map);
	err = sprd_iommu_iova_unmap(dev,iova,iova_length,handle);

	dev->map_count--;
	if (0 == dev->map_count)
		sprd_iommu_gsp_release(dev);
	mutex_unlock(&dev->mutex_map);

	return err;
}

int sprd_iommu_gsp_backup(struct sprd_iommu_dev *dev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	return 0;
#else
	int err = -1;

	mutex_lock(&dev->mutex_map);
	printk("%s, map_count:%d\n", __FUNCTION__, dev->map_count);
	if (dev->map_count > 0)
		err = sprd_iommu_backup(dev);
	mutex_unlock(&dev->mutex_map);

	return err;
#endif
}

int sprd_iommu_gsp_restore(struct sprd_iommu_dev *dev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	return 0;
#else
	int err = -1;

#ifdef GSP_IOMMU_WORKAROUND1
	sprd_iommu_gsp_enable_withworkaround(dev);
#endif

	mutex_lock(&dev->mutex_map);
	printk("%s, map_count:%d\n", __FUNCTION__, dev->map_count);
	if (dev->map_count > 0)
		err = sprd_iommu_restore(dev);
	mutex_unlock(&dev->mutex_map);

#ifdef GSP_IOMMU_WORKAROUND1
	sprd_iommu_gsp_disable(dev);
#endif
#endif

}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sprd_iommu_gsp_early_suspend(struct early_suspend* es)
{
	int err = -1;
	struct sprd_iommu_dev *dev = container_of(es, struct sprd_iommu_dev, early_suspend);

	mutex_lock(&dev->mutex_map);
	printk("%s, map_count:%d\n", __FUNCTION__, dev->map_count);
	if (dev->map_count > 0)
		err = sprd_iommu_backup(dev);
	mutex_unlock(&dev->mutex_map);
}

static void sprd_iommu_gsp_late_resume(struct early_suspend* es)
{
	int err = -1;
	struct sprd_iommu_dev *dev = container_of(es, struct sprd_iommu_dev, early_suspend);

#ifdef GSP_IOMMU_WORKAROUND1
	sprd_iommu_gsp_enable_withworkaround(dev);
#endif

	mutex_lock(&dev->mutex_map);
	printk("%s, map_count:%d\n", __FUNCTION__, dev->map_count);
	if (dev->map_count > 0)
		err = sprd_iommu_restore(dev);
	mutex_unlock(&dev->mutex_map);

#ifdef GSP_IOMMU_WORKAROUND1
		sprd_iommu_gsp_disable(dev);
#endif
}
#endif

int sprd_iommu_gsp_disable(struct sprd_iommu_dev *dev)
{
		pr_debug("%s line:%d\n",__FUNCTION__,__LINE__);
		sprd_iommu_disable(dev);
#if defined(CONFIG_ARCH_SCX30G)
	#ifdef CONFIG_OF
		clk_disable_unprepare(dev->mmu_clock);
		clk_disable_unprepare(dev->mmu_mclock);
	#else
		clk_disable(dev->mmu_clock);
		clk_disable(dev->mmu_mclock);
	#endif
#elif defined(CONFIG_ARCH_SCX15)
	#ifdef CONFIG_OF
		clk_disable_unprepare(dev->mmu_clock);
		clk_disable_unprepare(dev->mmu_mclock);
	#else
		clk_disable(dev->mmu_clock);
		clk_disable(dev->mmu_mclock);
	#endif
#endif
	return 0;
}


#ifdef GSP_IOMMU_WORKAROUND1
static void cycle_delay(uint32_t delay)
{
	while(delay--);
}

/*
func:gsp_iommu_workaround
desc:dolphin IOMMU workaround, configure GSP-IOMMU CTL REG before dispc_emc enable,
     including config IO base addr and enable gsp-iommu
warn:when dispc_emc disabled, reading ctl or entry register will hung up AHB bus .
     only 4-writting operations are allowed to exeute, over 4 ops will also hung uo AHB bus
     GSP module soft reset is not allowed , beacause it will clear gsp-iommu-ctrl register
*/
static void gsp_iommu_workaround(struct sprd_iommu_dev *dev)
{
	if(dev == NULL){
		printk("%s line:%d, dev==NULL, just return , without config gsp_iommu ctl reg!!!\n",__func__,__LINE__);
		return;
	}
	printk("%s line:%d REG_AON_APB_APB_EB1:0x%x\n",__func__,__LINE__,sci_glb_read(REG_AON_APB_APB_EB1,-1));
	if (!(sci_glb_read(REG_AON_APB_APB_EB1,-1) & 0x800)) {
		printk("%s line:%d dispc_emc not enable\n",__func__,__LINE__);
		__raw_writel(0x10000001, (void *)(dev->init_data->ctrl_reg));
	} else {
		printk("%s line:%d dispc_emc enabled\n",__func__,__LINE__);
		sci_glb_clr(REG_AON_APB_APB_EB1,0x800);
		//udelay(2);
		cycle_delay(5);
		__raw_writel(0x10000001, (void *)(dev->init_data->ctrl_reg));
		//udelay(2);
		cycle_delay(5);
		sci_glb_set(REG_AON_APB_APB_EB1,0x800);
	}
	printk("%s line:%d REG_AON_APB_APB_EB1:0x%x\n",__func__,__LINE__,sci_glb_read(REG_AON_APB_APB_EB1,-1));
}

int sprd_iommu_gsp_enable_withworkaround(struct sprd_iommu_dev *dev)

{
		printk("%s line:%d\n",__FUNCTION__,__LINE__);
#if defined(CONFIG_ARCH_SCX30G)
	#ifdef CONFIG_OF
		clk_prepare_enable(dev->mmu_clock);
		clk_prepare_enable(dev->mmu_mclock);
	#else
		clk_enable(dev->mmu_clock);
		clk_enable(dev->mmu_mclock);
	#endif
#elif defined(CONFIG_ARCH_SCX15)
	#ifdef CONFIG_OF
		
		clk_set_parent(dev->mmu_clock,dev->mmu_pclock);
		clk_prepare_enable(dev->mmu_clock);
		gsp_iommu_workaround(dev);
		clk_prepare_enable(dev->mmu_mclock);
	#else
		
		clk_set_parent(dev->mmu_clock,dev->mmu_pclock);
		clk_enable(dev->mmu_clock);
		gsp_iommu_workaround(dev);
		clk_enable(dev->mmu_mclock);
	#endif
	udelay(100);
#endif
		sprd_iommu_enable(dev);
		return 0;
}

#endif

int sprd_iommu_gsp_enable(struct sprd_iommu_dev *dev)
{
		pr_debug("%s line:%d\n",__FUNCTION__,__LINE__);
#if defined(CONFIG_ARCH_SCX30G)
	#ifdef CONFIG_OF
		clk_prepare_enable(dev->mmu_mclock);
		clk_prepare_enable(dev->mmu_clock);
	#else
		clk_enable(dev->mmu_mclock);
		clk_enable(dev->mmu_clock);
	#endif
#elif defined(CONFIG_ARCH_SCX15)
	#ifdef CONFIG_OF
		clk_prepare_enable(dev->mmu_mclock);
		clk_set_parent(dev->mmu_clock,dev->mmu_pclock);
		clk_prepare_enable(dev->mmu_clock);
	#else
		clk_enable(dev->mmu_mclock);
		clk_set_parent(dev->mmu_clock,dev->mmu_pclock);
		clk_enable(dev->mmu_clock);
	#endif
	udelay(100);
#endif
		sprd_iommu_enable(dev);
		return 0;
}

int sprd_iommu_gsp_dump(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length)
{
	return sprd_iommu_dump(dev,iova,iova_length);
}

struct sprd_iommu_ops iommu_gsp_ops={
	.init=sprd_iommu_gsp_init,
	.exit=sprd_iommu_gsp_exit,
	.iova_alloc=sprd_iommu_gsp_iova_alloc,
	.iova_free=sprd_iommu_gsp_iova_free,
	.iova_map=sprd_iommu_gsp_iova_map,
	.iova_unmap=sprd_iommu_gsp_iova_unmap,
	.backup=sprd_iommu_gsp_backup,
	.restore=sprd_iommu_gsp_restore,
	.disable=sprd_iommu_gsp_disable,
	.enable=sprd_iommu_gsp_enable,
	.dump=sprd_iommu_gsp_dump,
};

