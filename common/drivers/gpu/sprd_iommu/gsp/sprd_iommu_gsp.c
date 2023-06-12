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
#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

#define GSP_IOMMU_PGT_SIZE 1024	 // must be 2^n, such as 512, 1024, 2048 etc. 1024 is now enough for 1080p
uint32_t gsp_iommu_pgt[GSP_IOMMU_PGT_SIZE];

#ifdef CONFIG_ARCH_SCX15
#define GSP_IOMMU_WORKAROUND1
#endif


#ifdef GSP_IOMMU_WORKAROUND1
int sprd_iommu_gsp_enable_withworkaround(struct sprd_iommu_dev *dev);
#endif

int sprd_iommu_gsp_enable(struct sprd_iommu_dev *dev);
int sprd_iommu_gsp_disable(struct sprd_iommu_dev *dev);
extern unsigned int get_phys_addr(struct scatterlist *sg);

int sprd_iommu_gsp_init(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data)
{
	dev->mmu_mclock= clk_get(NULL,"clk_disp_emc");
	dev->mmu_pclock= clk_get(NULL,"clk_153m6");
	dev->mmu_clock=clk_get(NULL,"clk_gsp");
	if((NULL==dev->mmu_mclock)||(NULL==dev->mmu_pclock)||(NULL==dev->mmu_clock))
		return -1;
	sprd_iommu_gsp_enable(dev);

	pr_debug("sprd_iommu %s iova_base:0x%lx, iova_size:0x%x, pgt_base:0x%lx, pgt_size:0x%x",
		dev->init_data->name,data->iova_base,data->iova_size,data->pgt_base,data->pgt_size);
	dev->pgt=__get_free_pages(GFP_KERNEL,get_order(data->pgt_size));

	if(!dev->pgt)
	{
		pr_err("sprd_iommu %s get_free_pages fail\n",dev->init_data->name);
		return -1;
	}
	dev->pool = gen_pool_create(12, -1);
	if (!dev->pool) {
		free_pages((unsigned long)dev->pgt, get_order(data->pgt_size));
		pr_err("sprd_iommu %s gen_pool_create err\n",dev->init_data->name);
		return -1;
	}
	gen_pool_add(dev->pool, data->iova_base, data->iova_size, -1);

	return 0;
}

int sprd_iommu_gsp_exit(struct sprd_iommu_dev *dev)
{
	gen_pool_destroy(dev->pool);
	free_pages(dev->pgt, get_order(dev->init_data->pgt_size));
	dev->pgt=0;
	sprd_iommu_gsp_disable(dev);
	return 0;
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
	struct sg_table *table = handle->sg_table;
	struct scatterlist *sg;
	int i=0,iova_cur=0;
	uint32_t pgt_update_end = SPRD_IOMMU_PTE_ENTRY(iova),paddr=0;;
	int pgt_idx=0;
	unsigned long start_iova=0,end_iova=0;

	return sprd_iommu_iova_map(dev,iova,iova_length,handle);

}

int sprd_iommu_gsp_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	return sprd_iommu_iova_unmap(dev,iova,iova_length,handle);

}

int sprd_iommu_gsp_backup(struct sprd_iommu_dev *dev)
{
	sprd_iommu_backup(dev);
	sprd_iommu_gsp_disable(dev);
	return 0;
}

int sprd_iommu_gsp_restore(struct sprd_iommu_dev *dev)
{
	int i=0,pgt_idx=0;
#ifdef GSP_IOMMU_WORKAROUND1
	sprd_iommu_gsp_enable_withworkaround(dev);
#endif
	return sprd_iommu_restore(dev);
}

int sprd_iommu_gsp_disable(struct sprd_iommu_dev *dev)
{
	sprd_iommu_disable(dev);
	clk_disable(dev->mmu_clock);
	clk_disable(dev->mmu_mclock);
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
		__raw_writel(0x10000001, dev->init_data->ctrl_reg);
	} else {
		printk("%s line:%d dispc_emc enabled:0x%x\n",__func__,__LINE__);
		sci_glb_clr(REG_AON_APB_APB_EB1,0x800);
		udelay(2);
		__raw_writel(0x10000001, dev->init_data->ctrl_reg);
		udelay(2);
		sci_glb_set(REG_AON_APB_APB_EB1,0x800);
	}
	printk("%s line:%d REG_AON_APB_APB_EB1:0x%x\n",__func__,__LINE__,sci_glb_read(REG_AON_APB_APB_EB1,-1));
}

int sprd_iommu_gsp_enable_withworkaround(struct sprd_iommu_dev *dev)
{
	clk_set_parent(dev->mmu_clock,dev->mmu_pclock);
	clk_enable(dev->mmu_clock);
	gsp_iommu_workaround(dev);
	clk_enable(dev->mmu_mclock);
	udelay(300);
	sprd_iommu_enable(dev);
	return 0;
}

#endif


int sprd_iommu_gsp_enable(struct sprd_iommu_dev *dev)
{
	clk_set_parent(dev->mmu_clock,dev->mmu_pclock);
	clk_enable(dev->mmu_clock);
	clk_enable(dev->mmu_mclock);
	udelay(300);
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

