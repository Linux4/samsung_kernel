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

	if((0==iova)||(0==iova_length)||IS_ERR(handle))
	{
		pr_err("sprd_iommu %s sprd_iommu_iova_cur_map iova:0x%lx iova_length:0x%x handle:0x%p\n",dev->init_data->name,iova,iova_length,handle);
		return -1;
	}
	iova_cur=iova;
	mutex_lock(&dev->mutex_pgt);
	//disable TLB enable in MMU ctrl register
	//enable TLB
	for_each_sg(table->sgl, sg, table->nents, i) {
		start_iova=iova_cur;
		end_iova=iova_cur+PAGE_ALIGN(sg_dma_len(sg));
		paddr = get_phys_addr(sg);
		for(;start_iova<end_iova;start_iova+=SPRD_IOMMU_PAGE_SIZE,paddr+=SPRD_IOMMU_PAGE_SIZE)
		{
			gsp_iommu_pgt[SPRD_IOMMU_PTE_ENTRY(start_iova)] = paddr;
			pgt_update_end++;
		}
		iova_cur += PAGE_ALIGN(sg_dma_len(sg));
		if (iova_cur >=(iova+iova_length))
			break;
	}

	if (pgt_update_end > GSP_IOMMU_PGT_SIZE) {
		WARN(1, "sprd_iommu %s number of page table entries (%d) exceeds the limit (1024)\n", dev->init_data->name, pgt_update_end);
	}

	for (i=0; i<GSP_IOMMU_PGT_SIZE; i++) {
		//the write sequence of pte address is managed in grey code
		//after each write operation of pte address, clear the "address FIFO" of MMU
		pgt_idx = (i >> 1) ^ i;
		__raw_writel(gsp_iommu_pgt[pgt_idx], (volatile uint32_t*)(dev->init_data->pgt_base+(pgt_idx<<2)));
		//clear the "address FIFO" of MMU
		__raw_writel(gsp_iommu_pgt[0], (volatile uint32_t*)dev->init_data->pgt_base+0x0000);
		__raw_writel(gsp_iommu_pgt[0], (volatile uint32_t*)dev->init_data->pgt_base+0x0000);
		__raw_writel(gsp_iommu_pgt[0], (volatile uint32_t*)dev->init_data->pgt_base+0x0000);
	}
	mutex_unlock(&dev->mutex_pgt);

	pr_debug("sprd_iommu %s iova_map iova:0x%lx, iova_length:0x%x count:0x%x\n",dev->init_data->name,iova,iova_length,handle->iomap_cnt[dev->init_data->id]);
	return 0;
}

int sprd_iommu_gsp_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	if((0==iova)||(0==iova_length)||IS_ERR(handle))
	{
		pr_err("sprd_iommu %s sprd_iommu_iova_unmap iova:0x%lx iova_length:0x%x handle:0x%p\n",dev->init_data->name,iova,iova_length,handle);
		return -1;
	}
	pr_debug("sprd_iommu %s iova_unmap iova:0x%lx, iova_length:0x%x count:0x%x\n",dev->init_data->name,iova,iova_length,handle->iomap_cnt[dev->init_data->id]);
	return 0;
}

int sprd_iommu_gsp_backup(struct sprd_iommu_dev *dev)
{
	sprd_iommu_gsp_disable(dev);
	return 0;
}

int sprd_iommu_gsp_restore(struct sprd_iommu_dev *dev)
{
	int i=0,pgt_idx=0;
#ifdef CONFIG_ARCH_SCX15
	printk("%s line:%d REG_AON_APB_APB_EB1:0x%x\n",__func__,__LINE__,sci_glb_read(REG_AON_APB_APB_EB1,-1));
	if (!(sci_glb_read(REG_AON_APB_APB_EB1,-1) & 0x800)) {
		printk("%s line:%d dispc_emc not enable\n",__func__,__LINE__);
		__raw_writel(0x10000001, dev->init_data->ctrl_reg);
	}
	else
	{
		printk("%s line:%d dispc_emc enabled:0x%x\n",__func__,__LINE__);
		sci_glb_clr(REG_AON_APB_APB_EB1,0x800);
		udelay(2);
		__raw_writel(0x10000001, dev->init_data->ctrl_reg);
		udelay(2);
		sci_glb_set(REG_AON_APB_APB_EB1,0x800);
	}
	printk("%s line:%d REG_AON_APB_APB_EB1:0x%x\n",__func__,__LINE__,sci_glb_read(REG_AON_APB_APB_EB1,-1));
#endif
	sprd_iommu_gsp_enable(dev);

	mutex_lock(&dev->mutex_pgt);
	for (i=0; i<GSP_IOMMU_PGT_SIZE; i++) {
		//the write sequence of pte address is managed in grey code
		//after each write operation of pte address, clear the "address FIFO" of MMU
		pgt_idx = (i >> 1) ^ i;
		__raw_writel(gsp_iommu_pgt[pgt_idx], (volatile uint32_t*)(dev->init_data->pgt_base+(pgt_idx<<2)));
		//clear the "address FIFO" of MMU
		__raw_writel(gsp_iommu_pgt[0], (volatile uint32_t*)dev->init_data->pgt_base+0x0000);
		__raw_writel(gsp_iommu_pgt[0], (volatile uint32_t*)dev->init_data->pgt_base+0x0000);
		__raw_writel(gsp_iommu_pgt[0], (volatile uint32_t*)dev->init_data->pgt_base+0x0000);
	}
	mutex_unlock(&dev->mutex_pgt);
	return 0;
}

int sprd_iommu_gsp_disable(struct sprd_iommu_dev *dev)
{
	sprd_iommu_disable(dev);
	clk_disable(dev->mmu_clock);
	clk_disable(dev->mmu_mclock);
	return 0;
}

int sprd_iommu_gsp_enable(struct sprd_iommu_dev *dev)
{
	clk_enable(dev->mmu_mclock);
	clk_set_parent(dev->mmu_clock,dev->mmu_pclock);
	clk_enable(dev->mmu_clock);
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

