#include "sprd_iommu_common.h"

inline void mmu_reg_write(ulong reg, ulong val, ulong msk)
{
	__raw_writel((__raw_readl((void *)reg) & ~msk) | val, (void *)reg);
}

static inline void __set_pte(struct sprd_iommu_init_data *data,
		ulong base, u32 offset, ulong paddr)
{
	/*64-bit virtual addr and 32bit phy addr in arm64.*/
	if (data->iommu_rev == 2)
		((u32 *)base)[offset] = (u32)(paddr >> 12);
	else
		((u32 *)base)[offset] = (u32)paddr;
}

ulong get_phys_addr(struct scatterlist *sg)
{
	/*
	 * Try sg_dma_address first so that we can
	 * map carveout regions that do not have a
	 * struct page associated with them.
	 */
	ulong pa = sg_dma_address(sg);
	if (pa == 0)
		pa = sg_phys(sg);
	return pa;
}

static void sprd_iommu_pgt_reset(struct sprd_iommu_init_data *data)
{
	ulong fault_page = data->fault_page;
	int i;

	if (fault_page) {
		for (i = 0; i < data->pgt_size / sizeof(u32); i++)
			__set_pte(data, data->pgt_base, i, fault_page);
	} else {
		memset((void *)data->pgt_base, 0xFF, data->pgt_size);
	}
}

static inline void sprd_iommu_pgt_update(struct sprd_iommu_init_data *data,
				ulong iova, ulong pa, size_t size)
{
	ulong end_iova = iova + size;
	for (; iova < end_iova; iova += MMU_PAGE_SIZE, pa += MMU_PAGE_SIZE) {
		__set_pte(data, data->pgt_base, MMU_PTE_OFFSET(iova), pa);
	}
}

static inline void sprd_iommu_pgt_clear(struct sprd_iommu_init_data *data,
				ulong iova, size_t size)
{
	ulong pgt_start = (ulong)((u32 *)data->pgt_base + MMU_PTE_OFFSET(iova));

	pr_debug("%s, iova:0x%lx, size:0x%zx, fault_page:0x%lx\n", __func__,
		iova, size, data->fault_page);

	if (data->fault_page) {
		ulong end_iova = iova + size;
		ulong fault_page = data->fault_page;

		for (; iova < end_iova; iova += MMU_PAGE_SIZE)
			__set_pte(data, data->pgt_base, MMU_PTE_OFFSET(iova),
				fault_page);
	} else {
		if (pgt_start % sizeof(ulong)) {
			*((u32 *)pgt_start) =  (u32)-1;
			memset((void *)((u32 *)pgt_start + 1), 0xFF,
				MMU_IOVA_SIZE_TO_PTE(size - MMU_PAGE_SIZE));
		} else {
			memset((void *)pgt_start, 0xFF,
				MMU_IOVA_SIZE_TO_PTE(size));
		}
        }

}

static void sprd_iommu_reg_init(struct sprd_iommu_init_data *data)
{
	if (data->re_route_page) {
		mmu_reg_write(data->ctrl_reg + OFFSET_RR_DEST_HI,
				(u32)(data->re_route_page >> 32),
				MMU_START_MB_ADDR_HI_MASK);
		mmu_reg_write(data->ctrl_reg + OFFSET_RR_DEST_LO,
				(u32)(data->re_route_page),
				MMU_START_MB_ADDR_LO_MASK);
		mmu_reg_write(data->ctrl_reg + OFFSET_CFG,
				MMU_RR_EN(1),
				MMU_RR_EN_MASK);
	}

	if (data->iommu_rev == 2) {
		mmu_reg_write(data->ctrl_reg + OFFSET_START_MB_ADDR_HI,
				(u32)(data->iova_base >> 32),
				MMU_START_MB_ADDR_HI_MASK);
		mmu_reg_write(data->ctrl_reg + OFFSET_START_MB_ADDR_LO,
				(u32)(data->iova_base),
				MMU_START_MB_ADDR_LO_MASK);

		mmu_reg_write(data->ctrl_reg + OFFSET_CFG,
				MMU_MON_EN(1),
				MMU_MON_EN_MASK);
	} else {
#ifdef CONFIG_ARCH_SCX35L
		mmu_reg_write(data->ctrl_reg,
				MMU_V1_RAMCLK_DIV2_EN(1),
				MMU_V1_RAMCLK_DIV2_EN_MASK);
#endif
		mmu_reg_write(data->ctrl_reg,
				data->iova_base,
				MMU_START_MB_ADDR_MASK);
	}

	mmu_reg_write(data->ctrl_reg, MMU_TLB_EN(1), MMU_TLB_EN_MASK);
	mmu_reg_write(data->ctrl_reg, MMU_EN(1), MMU_EN_MASK);
}

static void sprd_iommu_reg_clear(struct sprd_iommu_init_data *data)
{
	if (data->iommu_rev == 2) {
		mmu_reg_write(data->ctrl_reg + OFFSET_CFG,
				MMU_RR_EN(0),
				MMU_RR_EN_MASK);
		mmu_reg_write(data->ctrl_reg + OFFSET_CFG,
				MMU_MON_EN(0),
				MMU_MON_EN_MASK);
	} else {
		mmu_reg_write(data->ctrl_reg,
				MMU_V1_RAMCLK_DIV2_EN(0),
				MMU_V1_RAMCLK_DIV2_EN_MASK);
	}

	mmu_reg_write(data->ctrl_reg, MMU_TLB_EN(0), MMU_TLB_EN_MASK);
	mmu_reg_write(data->ctrl_reg, MMU_EN(0), MMU_EN_MASK);
}

int sprd_iommu_init(struct sprd_iommu_dev *dev,
		struct sprd_iommu_init_data *data)
{
	int i;

	pr_info("%s, %s, id:%d, iova_base:0x%lx, size:0x%zx\n", __func__,
		data->name, data->id, data->iova_base, data->iova_size);

	pr_info("pgt_base:0x%lx,size:0x%zx, fault_page:0x%lx, rr_page:0x%lx\n",
		data->pgt_base, data->pgt_size, data->fault_page,
		data->re_route_page);

	if (data->fault_page) {
		void *p = __va(data->fault_page);
		pr_info("fault_page va: %p", p);
		for (i = 0; i < PAGE_SIZE / sizeof(u32); i++)
			((u32 *)p)[i] = 0xDEADDEAD;
	}
	sprd_iommu_pgt_reset(data);

	dev->pgt = __get_free_pages(GFP_KERNEL, get_order(data->pgt_size));
	if (!dev->pgt) {
		pr_err("%s get_free_pages fail\n", data->name);
		return -1;
	}

	dev->pool = gen_pool_create(12, -1);
	if (!dev->pool) {
		free_pages((ulong)dev->pgt, get_order(data->pgt_size));
		pr_err("%s gen_pool_create error\n", data->name);
		return -1;
	}
	gen_pool_add(dev->pool, data->iova_base, data->iova_size, -1);

	if (data->re_route_page) {
		void *p = __va(data->re_route_page);
		pr_info("re_route_page va: %p", p);
		for (i = 0; i < PAGE_SIZE / sizeof(u32); i++)
			((u32 *)p)[i] = 0xDEADDEAD;
	}

	sprd_iommu_reg_init(data);

	if (data->iommu_rev == 2) {
		pr_info("ctrl_reg: 0x%x,cfg_reg: 0x%x,start_mb_addr:0x%x 0x%x",
		__raw_readl((void *)(data->ctrl_reg)),
		__raw_readl((void *)(data->ctrl_reg + OFFSET_CFG)),
		__raw_readl((void *)(data->ctrl_reg + OFFSET_START_MB_ADDR_HI)),
		__raw_readl((void *)(data->ctrl_reg + OFFSET_START_MB_ADDR_LO)));

		pr_info("param_reg: 0x%x, rev_reg: 0x%x",
		__raw_readl((void *)(data->ctrl_reg + OFFSET_PARAM)),
		__raw_readl((void *)(data->ctrl_reg + OFFSET_REV)));
	}

	return 0;
}

int sprd_iommu_exit(struct sprd_iommu_dev *dev)
{
	sprd_iommu_reg_clear(dev->init_data);
	gen_pool_destroy(dev->pool);
	free_pages(dev->pgt, get_order(dev->init_data->pgt_size));
	dev->pgt = 0;
	return 0;
}

ulong sprd_iommu_iova_alloc(struct sprd_iommu_dev *dev, size_t iova_length)
{
	ulong iova = 0;

	if (0 == iova_length) {
		pr_err("%s, %s, iova_length:0x%zx\n", __func__,
			dev->init_data->name, iova_length);
		return 0;
	}

	iova =  gen_pool_alloc(dev->pool, iova_length);
	pr_debug("%s, %s, iova:0x%lx, iova_length:0x%zx\n", __func__,
		dev->init_data->name, iova, iova_length);
	if (0 == iova) {
		pr_err("%s, %s, iova:0x%lx, iova_length:0x%zx\n", __func__,
			dev->init_data->name, iova, iova_length);
	}

	return iova;
}

void sprd_iommu_iova_free(struct sprd_iommu_dev *dev,
			ulong iova, size_t iova_length)
{
	struct sprd_iommu_init_data *data = dev->init_data;

	if (((data->iova_base + data->iova_size) < (iova + iova_length))
	|| (data->iova_base > iova) || (0 == iova) || (0 == iova_length)) {
		pr_err("%s, %s, iova_base:0x%lx, iova_size:0x%zx\n",
			__func__, data->name, data->iova_base, data->iova_size);
		pr_err("iova:0x%lx, len:0x%zx\n", iova, iova_length);
		return;
	}
	gen_pool_free(dev->pool, iova, iova_length);
	pr_debug("%s, %s, iova:0x%lx, len:0x%zx\n", __func__,
		data->name, iova, iova_length);
}

int sprd_iommu_iova_map(struct sprd_iommu_dev *dev, ulong iova,
			size_t iova_length, struct sg_table *table)
{
	struct scatterlist *sg;
	int i = 0;
	ulong iova_cur = 0;

	if ((0 == iova) || (0 == iova_length)) {
		pr_err("%s, %s, iova:0x%lx, iova_length:0x%zx\n", __func__,
			dev->init_data->name, iova, iova_length);
		return -1;
	}
	iova_cur=iova;
	mutex_lock(&dev->mutex_pgt);
	/*disable TLB enable in MMU ctrl register*/
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
	for_each_sg(table->sgl, sg, table->nents, i) {
		sprd_iommu_pgt_update(dev->init_data, iova_cur,
				get_phys_addr(sg), PAGE_ALIGN(sg->length));
		iova_cur += PAGE_ALIGN(sg->length);
		if (iova_cur >= iova + iova_length)
			break;
	}
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
	mutex_unlock(&dev->mutex_pgt);

	pr_debug("%s, %s, iova:0x%lx, iova_length:0x%zx\n", __func__,
		dev->init_data->name, iova, iova_length);

	return 0;
}

int sprd_iommu_iova_unmap(struct sprd_iommu_dev *dev, ulong iova,
			size_t iova_length)
{
	if ((0 == iova) || (0 == iova_length)) {
		pr_err("%s, %s, iova:0x%lx, iova_length:0x%zx\n", __func__,
			dev->init_data->name, iova, iova_length);
		return -1;
	}

	mutex_lock(&dev->mutex_pgt);
	/*disable TLB enable in MMU ctrl register*/
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(0),MMU_TLB_EN_MASK);
	sprd_iommu_pgt_clear(dev->init_data, iova, iova_length);
	mmu_reg_write(dev->init_data->ctrl_reg,MMU_TLB_EN(1),MMU_TLB_EN_MASK);
	mutex_unlock(&dev->mutex_pgt);

	pr_debug("%s, %s, iova:0x%lx, iova_length:0x%zx\n", __func__,
		dev->init_data->name, iova, iova_length);

	return 0;
}

int sprd_iommu_backup(struct sprd_iommu_dev *dev)
{
	mutex_lock(&dev->mutex_pgt);
	memcpy((ulong *)dev->pgt, (ulong *)dev->init_data->pgt_base,
		dev->init_data->pgt_size);
	sprd_iommu_reg_clear(dev->init_data);
	mutex_unlock(&dev->mutex_pgt);

	return 0;
}

int sprd_iommu_restore(struct sprd_iommu_dev *dev)
{
	mutex_lock(&dev->mutex_pgt);
	mmu_reg_write(dev->init_data->ctrl_reg, MMU_EN(0), MMU_EN_MASK);
	memcpy((ulong *)dev->init_data->pgt_base, (ulong *)dev->pgt,
		dev->init_data->pgt_size);
	sprd_iommu_reg_init(dev->init_data);
	mutex_unlock(&dev->mutex_pgt);

	return 0;
}

void sprd_iommu_disable(struct sprd_iommu_dev *dev)
{
	mutex_lock(&dev->mutex_pgt);
	sprd_iommu_reg_clear(dev->init_data);
	mutex_unlock(&dev->mutex_pgt);
}

void sprd_iommu_enable(struct sprd_iommu_dev *dev)
{
	mutex_lock(&dev->mutex_pgt);
	mmu_reg_write(dev->init_data->ctrl_reg, MMU_EN(0), MMU_EN_MASK);
	sprd_iommu_reg_init(dev->init_data);
	mutex_unlock(&dev->mutex_pgt);
}

void sprd_iommu_reset_enable(struct sprd_iommu_dev *dev)
{
	mutex_lock(&dev->mutex_pgt);
	mmu_reg_write(dev->init_data->ctrl_reg, MMU_EN(0), MMU_EN_MASK);
	sprd_iommu_pgt_reset(dev->init_data);
	sprd_iommu_reg_init(dev->init_data);
	mutex_unlock(&dev->mutex_pgt);
}

int sprd_iommu_dump(struct sprd_iommu_dev *dev, ulong iova, size_t iova_length)
{
	return 0;
}
