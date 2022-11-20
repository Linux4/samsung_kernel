#include "sprd_iommu_common.h"
#ifdef GSP_IOMMU_WORKAROUND1
//extern struct sprd_iommu_ops iommu_gsp_ops;
extern struct sprd_iommu_ops iommu_mm_ops;
#endif

inline void mmu_reg_write(unsigned long reg, unsigned long val, unsigned long msk)
{
	__raw_writel((__raw_readl((void *)reg) & ~msk) | val, (void *)reg);
}
unsigned int get_phys_addr(struct scatterlist *sg)
{
	/*
	 * Try sg_dma_address first so that we can
	 * map carveout regions that do not have a
	 * struct page associated with them.
	 */
	unsigned int pa = sg_dma_address(sg);
	if (pa == 0)
		pa = sg_phys(sg);
	return pa;
}

static inline void sprd_iommu_update_pgt(unsigned long pgt_base, unsigned long iova, uint32_t paddr, size_t size)
{
	unsigned long end_iova=iova+size;
	for(;iova<end_iova;iova+=SPRD_IOMMU_PAGE_SIZE,paddr+=SPRD_IOMMU_PAGE_SIZE)
	{
		//printk("sprd_iommu pgt_base:0x%x offset:0x%x iova:0x%x paddr:0x%x\n",pgt_base,SPRD_IOMMU_PTE_ENTRY(iova),iova,paddr);
		*((unsigned long *)(pgt_base)+SPRD_IOMMU_PTE_ENTRY(iova))=paddr;
		//printk("sprd_iommu pgt_addr:0x%x paddr:0x%x\n",
		//	(uint32_t)((uint32_t *)(pgt_base)+SPRD_IOMMU_PTE_ENTRY(iova)),*((uint32_t *)(pgt_base)+SPRD_IOMMU_PTE_ENTRY(iova)) );
	}
}

static inline void sprd_iommu_clear_pgt(unsigned long pgt_base, unsigned long iova, size_t size)
{
	pr_debug("sprd_iommu pgt_base:0x%lx offset:0x%lx iova:0x%lx size:0x%x\n",pgt_base,SPRD_IOMMU_PTE_ENTRY(iova),iova,size);
	memset((unsigned long *)pgt_base+SPRD_IOMMU_PTE_ENTRY(iova),0xFFFFFFFF,(size>>10));
}

int sprd_iommu_init(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data)
{
	pr_debug("sprd_iommu %s iova_base:0x%lx, iova_size:0x%x, pgt_base:0x%lx, pgt_size:0x%x",
		dev->init_data->name,data->iova_base,data->iova_size,data->pgt_base,data->pgt_size);
	dev->pgt=__get_free_pages(GFP_KERNEL,get_order(data->pgt_size));
	memset((unsigned long *)data->pgt_base,0xFFFFFFFF,PAGE_ALIGN(data->pgt_size));
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
	//write start_addr to MMU_CTL register
	//TLB enable
	//MMU enable
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(1),MMU_RAMCLK_DIV2_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,dev->init_data->iova_base,MMU_START_MB_ADDR_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(1),MMU_EN_MASK);

	return 0;
}

int sprd_iommu_exit(struct sprd_iommu_dev *dev)
{
	//TLB disable
	//MMU disable
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(0),MMU_RAMCLK_DIV2_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
	gen_pool_destroy(dev->pool);
	free_pages(dev->pgt, get_order(dev->init_data->pgt_size));
	dev->pgt=0;
	return 0;
}

unsigned long sprd_iommu_iova_alloc(struct sprd_iommu_dev *dev, size_t iova_length)
{
	unsigned long iova = 0;

	if(0==iova_length)
	{
		pr_err("sprd_iommu %s sprd_iommu_iova_alloc iova_length:0x%x\n",dev->init_data->name,iova_length);
		return 0;
	}

	iova =  gen_pool_alloc(dev->pool, iova_length);
	pr_debug("sprd_iommu %s iova_alloc iova:0x%lx, iova_length:0x%x\n",dev->init_data->name,iova,iova_length);
	if (0 == iova) {
		pr_err("sprd_iommu %s iova_alloc iova_base: iova:0x%lx iova_length:0x%x\n",dev->init_data->name,iova,iova_length);
	}

	return (iova);
}

void sprd_iommu_iova_free(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length)
{
	if(((dev->init_data->iova_base+dev->init_data->iova_size)<(iova+iova_length))||(0==iova)||(0==iova_length))
	{
		pr_err("sprd_iommu %s iova_free iova_base:0x%lx iova_size:0x%x iova:0x%lx iova_length:0x%x\n",
			dev->init_data->name,dev->init_data->iova_base,dev->init_data->iova_size,iova,iova_length);
		return;
	}
	gen_pool_free(dev->pool, iova, iova_length);
	pr_debug("sprd_iommu %s iova_free iova:0x%lx, iova_length:0x%x\n",dev->init_data->name,iova,iova_length);
}

int sprd_iommu_iova_map(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	struct sg_table *table = handle->sg_table;
	struct scatterlist *sg;
	int i=0,iova_cur=0;

	if((0==iova)||(0==iova_length)||IS_ERR(handle))
	{
		pr_err("sprd_iommu %s sprd_iommu_iova_cur_map iova:0x%lx iova_length:0x%x handle:0x%p\n",dev->init_data->name,iova,iova_length,handle);
		return -1;
	}
	iova_cur=iova;
	mutex_lock(&dev->mutex_pgt);
	//disable TLB enable in MMU ctrl register
	//enable TLB
#ifdef GSP_IOMMU_WORKAROUND1
	if(dev->ops == &iommu_mm_ops){
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
	}
#else
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
#endif
	for_each_sg(table->sgl, sg, table->nents, i) {
		sprd_iommu_update_pgt(dev->init_data->pgt_base,iova_cur,get_phys_addr(sg),PAGE_ALIGN(sg_dma_len(sg)));
		iova_cur += PAGE_ALIGN(sg_dma_len(sg));
		if (iova_cur >=(iova+iova_length))
			break;
	}
#ifdef GSP_IOMMU_WORKAROUND1
	if(dev->ops == &iommu_mm_ops){
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
	}
#else
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
#endif
	mutex_unlock(&dev->mutex_pgt);

	pr_debug("sprd_iommu %s iova_map iova:0x%lx, iova_length:0x%x count:0x%x\n",dev->init_data->name,iova,iova_length,handle->iomap_cnt[dev->init_data->id]);
	return 0;
}

int sprd_iommu_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle)
{
	if((0==iova)||(0==iova_length)||IS_ERR(handle))
	{
		pr_err("sprd_iommu %s sprd_iommu_iova_unmap iova:0x%lx iova_length:0x%x handle:0x%p\n",dev->init_data->name,iova,iova_length,handle);
		return -1;
	}

	mutex_lock(&dev->mutex_pgt);
	//disable TLB enable in MMU ctrl register
	//enable TLB
#ifdef GSP_IOMMU_WORKAROUND1
	if(dev->ops == &iommu_mm_ops){
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
	}
#else
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
#endif
	sprd_iommu_clear_pgt(dev->init_data->pgt_base,iova,iova_length);
#ifdef GSP_IOMMU_WORKAROUND1
	if(dev->ops == &iommu_mm_ops){
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
	}
#else
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
#endif
	mutex_unlock(&dev->mutex_pgt);

	pr_debug("sprd_iommu %s iova_unmap iova:0x%lx, iova_length:0x%x count:0x%x\n",dev->init_data->name,iova,iova_length,handle->iomap_cnt[dev->init_data->id]);
	return 0;
}

int sprd_iommu_backup(struct sprd_iommu_dev *dev)
{
	mutex_lock(&dev->mutex_pgt);
	memcpy((unsigned long*)dev->pgt,(unsigned long*)dev->init_data->pgt_base,PAGE_ALIGN(dev->init_data->pgt_size));
#ifdef GSP_IOMMU_WORKAROUND1
	if(dev->ops == &iommu_mm_ops){
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(0),MMU_RAMCLK_DIV2_EN_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
	}
#else
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(0),MMU_RAMCLK_DIV2_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
#endif
	mutex_unlock(&dev->mutex_pgt);
	return 0;
}

int sprd_iommu_restore(struct sprd_iommu_dev *dev)
{
	mutex_lock(&dev->mutex_pgt);
#ifdef GSP_IOMMU_WORKAROUND1
	if(dev->ops == &iommu_mm_ops){
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
	}
#else
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(0),MMU_EN_MASK);
#endif
	memcpy((unsigned long*)dev->init_data->pgt_base,(unsigned long*)dev->pgt,PAGE_ALIGN(dev->init_data->pgt_size));
#ifdef GSP_IOMMU_WORKAROUND1
	if(dev->ops == &iommu_mm_ops){
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(1),MMU_RAMCLK_DIV2_EN_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,dev->init_data->iova_base,MMU_START_MB_ADDR_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
		mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(1),MMU_EN_MASK);
	}
#else
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_RAMCLK_DIV2_EN(1),MMU_RAMCLK_DIV2_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,dev->init_data->iova_base,MMU_START_MB_ADDR_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_EN(1),MMU_EN_MASK);
#endif
	mutex_unlock(&dev->mutex_pgt);
	return 0;
}

int sprd_iommu_disable(struct sprd_iommu_dev *dev)
{
	return 0;
}

int sprd_iommu_enable(struct sprd_iommu_dev *dev)
{
	return 0;
}

int sprd_iommu_dump(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length)
{
	return 0;
}
