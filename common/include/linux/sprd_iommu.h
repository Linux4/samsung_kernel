#ifndef _SPRD_IOMMU_H
#define _SPRD_IOMMU_H

#include <linux/ion.h>
#include <linux/miscdevice.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define MMU_START_MB_ADDR(_x_)		( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define MMU_RAMCLK_DIV2_EN(_x_)		( (_x_) << 2 & (BIT(2)) )
#define MMU_TLB_EN(_x_)				( (_x_) << 1 & (BIT(1)) )
#define MMU_EN(_x_)					( (_x_) << 0 & (BIT(0)) )
#define MMU_START_MB_ADDR_MASK		( (BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define MMU_RAMCLK_DIV2_EN_MASK		( (BIT(2)) )
#define MMU_TLB_EN_MASK				( (BIT(1)) )
#define MMU_EN_MASK					( (BIT(0)) )

struct sprd_iommu_init_data {
	int id;                        /* iommu id */
	char *name;                    /* iommu name */
	
	unsigned long iova_base;            /* io virtual address base */
	size_t iova_size;            /* io virtual address size */
	unsigned long pgt_base;             /* iommu page table base address */
	size_t pgt_size;             /* iommu page table array size */
	unsigned int ctrl_reg;             /* iommu control register */
};

struct sprd_iommu_dev {
	struct miscdevice misc_dev;
	struct sprd_iommu_init_data *init_data;
	struct gen_pool *pool;
	struct sprd_iommu_ops *ops;
	unsigned long pgt;
	struct mutex mutex_pgt;
	struct clk* mmu_mclock;
	struct clk* mmu_pclock;
	struct clk* mmu_clock;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
	void *private;
	unsigned int map_count;
	struct mutex mutex_map;
};

struct sprd_iommu_ops {
	int (*init)(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data);
	int (*exit)(struct sprd_iommu_dev *dev);
	unsigned long (*iova_alloc)(struct sprd_iommu_dev *dev, size_t iova_length);
	void (*iova_free)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
	int (*iova_map)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle);
	int (*iova_unmap)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct ion_buffer *handle);
	int (*backup)(struct sprd_iommu_dev *dev);
	int (*restore)(struct sprd_iommu_dev *dev);
	int (*disable)(struct sprd_iommu_dev *dev);
	int (*enable)(struct sprd_iommu_dev *dev);
	int (*dump)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
};

enum IOMMU_ID {
	IOMMU_GSP = 0,
	IOMMU_MM,
	IOMMU_MAX,
};

extern unsigned long sprd_iova_alloc(int iommu_id, unsigned long iova_length);
extern void sprd_iova_free(int iommu_id, unsigned long iova, unsigned long iova_length);
extern int sprd_iova_map(int iommu_id, unsigned long iova, struct ion_buffer *handle);
extern int sprd_iova_unmap(int iommu_id, unsigned long iova,  struct ion_buffer *handle);
extern void sprd_iommu_module_enable(uint32_t iommu_id);
extern void sprd_iommu_module_disable(uint32_t iommu_id);
#endif
