#ifndef _SPRD_IOMMU_H
#define _SPRD_IOMMU_H

#include <linux/scatterlist.h>
#include <linux/miscdevice.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

struct sprd_iommu_init_data {
	int id;                        /* iommu id */
	char *name;                    /* iommu name */
	
	unsigned long iova_base;            /* io virtual address base */
	size_t iova_size;            /* io virtual address size */
	unsigned long pgt_base;             /* iommu page table base address */
	size_t pgt_size;             /* iommu page table array size */
	unsigned long ctrl_reg;             /* iommu control register */
	unsigned long fault_page;
	unsigned long re_route_page;
	unsigned int iommu_rev;
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
	struct clk* mmu_axiclock;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
	void *private;
	unsigned int map_count;
	struct mutex mutex_map;
	unsigned int div2_frq;
	bool light_sleep_en;
};

struct sprd_iommu_ops {
	int (*init)(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data);
	int (*exit)(struct sprd_iommu_dev *dev);
	unsigned long (*iova_alloc)(struct sprd_iommu_dev *dev, size_t iova_length);
	void (*iova_free)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
	int (*iova_map)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct sg_table *table);
	int (*iova_unmap)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
	int (*backup)(struct sprd_iommu_dev *dev);
	int (*restore)(struct sprd_iommu_dev *dev);
	void (*enable)(struct sprd_iommu_dev *dev);
	void (*disable)(struct sprd_iommu_dev *dev);
	int (*dump)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
	void (*open)(struct sprd_iommu_dev *dev);
	void (*release)(struct sprd_iommu_dev *dev);
};

enum IOMMU_ID {
	IOMMU_GSP = 0,
	IOMMU_MM,
	/*for whale iommu*/
	IOMMU_VSP,
	IOMMU_DCAM,
	IOMMU_DISPC,
	IOMMU_GSP0,
	IOMMU_GSP1,
	IOMMU_VPP,
	IOMMU_MAX,
};

extern struct sprd_iommu_ops iommu_gsp_ops;
extern struct sprd_iommu_ops iommu_mm_ops;
/*for whale iommu*/
extern struct sprd_iommu_ops iommu_vsp_ops;
extern struct sprd_iommu_ops iommu_dcam_ops;
extern struct sprd_iommu_ops iommu_dispc_ops;
extern struct sprd_iommu_ops iommu_gsp0_ops;
extern struct sprd_iommu_ops iommu_gsp1_ops;
extern struct sprd_iommu_ops iommu_vpp_ops;

extern unsigned long sprd_iova_alloc(int iommu_id, unsigned long iova_length);
extern void sprd_iova_free(int iommu_id, unsigned long iova, unsigned long iova_length);
extern int sprd_iova_map(int iommu_id, unsigned long iova, unsigned long iova_length, struct sg_table *table);
extern int sprd_iova_unmap(int iommu_id, unsigned long iova, unsigned long iova_length);
extern void sprd_iommu_module_enable(uint32_t iommu_id);
extern void sprd_iommu_module_disable(uint32_t iommu_id);
#endif
