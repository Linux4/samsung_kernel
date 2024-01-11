#ifndef _SPRD_IOMMU_H
#define _SPRD_IOMMU_H

#include <linux/scatterlist.h>
#include <asm/cacheflush.h>

struct sprd_iommu_init_data {
	int id;                        /* iommu id */
	char *name;                    /* iommu name */

	unsigned long iova_base;            /* io virtual address base */
	size_t iova_size;            /* io virtual address size */
	unsigned long frc_reg_addr;  /* force copy reg address */
	size_t pgt_size;             /* iommu page table array size */
	unsigned long ctrl_reg;             /* iommu control register base */
	u32 *dump_regs;         /* iommu regs dump array*/
	size_t dump_size;        /* iommu regs dump range */
	unsigned long fault_page;

	/*iommu reserved memory of pf page table*/
	unsigned long pagt_base_ddr;
	unsigned long pagt_ddr_size;
};

#define SPRD_MAX_SG_CACHED_CNT 1024

enum sg_pool_status {
	SG_SLOT_FREE = 0,
	SG_SLOT_USED,
};

enum sprd_iommu_chtype {
	SPRD_IOMMU_PF_CH_READ = 0x100,/*prefetch channel only support read*/
	SPRD_IOMMU_PF_CH_WRITE,/* prefetch channel only support write*/
	SPRD_IOMMU_FM_CH_RW,/*fullmode channel support w/r in one channel*/
	SPRD_IOMMU_EX_CH_READ,/*channel only support read, only ISP use now*/
	SPRD_IOMMU_EX_CH_WRITE,/*channel only support read, only ISP use now*/
	SPRD_IOMMU_CH_TYPE_INVALID, /*unsupported channel type*/
};

enum sprd_iommu_id {
	SPRD_IOMMU_VSP,
	SPRD_IOMMU_VSP1,
	SPRD_IOMMU_VSP2,
	SPRD_IOMMU_DCAM,
	SPRD_IOMMU_DCAM1,
	SPRD_IOMMU_CPP,
	SPRD_IOMMU_GSP,
	SPRD_IOMMU_GSP1,
	SPRD_IOMMU_JPG,
	SPRD_IOMMU_DISP,
	SPRD_IOMMU_DISP1,
	SPRD_IOMMU_ISP,
	SPRD_IOMMU_ISP1,
	SPRD_IOMMU_FD,
	SPRD_IOMMU_NPU,
	SPRD_IOMMU_EPP,
	SPRD_IOMMU_EDP,
	SPRD_IOMMU_IDMA,
	SPRD_IOMMU_VDMA,
	SPRD_IOMMU_MAX,
};

struct sprd_iommu_sg_rec {
	unsigned long sg_table_addr;
	void *buf_addr;
	unsigned long iova_addr;
	unsigned long iova_size;
	enum sg_pool_status status;
	int map_usrs;
};

struct sprd_iommu_sg_pool {
	int pool_cnt;
	struct sprd_iommu_sg_rec slot[SPRD_MAX_SG_CACHED_CNT];
};

struct sprd_iommu_dev {
	struct sprd_iommu_init_data *init_data;
	struct gen_pool *pool;
	struct sprd_iommu_ops *ops;

	void *private;
	unsigned int map_count;
	spinlock_t pgt_lock;

	struct sprd_iommu_sg_pool sg_pool;
	int id;
	struct notifier_block sprd_iommu_nb;
};

/*map arguments for kernel space*/
struct sprd_iommu_map_data {
	struct sg_table *table;
	void *buf;
	size_t iova_size;
	enum sprd_iommu_chtype ch_type;
	unsigned int sg_offset;
	unsigned long iova_addr;/*output*/
};

struct sprd_iommu_unmap_data {
	unsigned long iova_addr;
	size_t iova_size;
	enum sprd_iommu_chtype ch_type;
	struct sg_table *table;
	void *buf;
	u32 channel_id;
};

struct sprd_iommu_list_data {
	enum sprd_iommu_id iommu_id;
	struct sprd_iommu_dev *iommu_dev;
};

/*kernel API for Iommu map/unmap*/
#if IS_ENABLED(CONFIG_SPRD_IOMMU)
int sprd_iommu_notifier_call_chain(void *data);
void sprd_iommu_pool_show(struct device *dev);
void sprd_iommu_reg_dump(struct device *dev);
void sprd_iommu_reg_show(struct device *dev);
int sprd_iommu_attach_device(struct device *dev);
int sprd_iommu_map(struct device *dev,
		struct sprd_iommu_map_data *data);
int sprd_iommu_map_single_page(struct device *dev,
		struct sprd_iommu_map_data *data);
int sprd_iommu_map_with_idx(struct device *dev,
		struct sprd_iommu_map_data *data, int idx);
int sprd_iommu_unmap(struct device *dev,
		struct sprd_iommu_unmap_data *data);
int sprd_iommu_unmap_with_idx(struct device *dev,
		struct sprd_iommu_unmap_data *data, int idx);
int sprd_iommu_restore(struct device *dev);
int sprd_iommu_set_cam_bypass(bool vaor_bp_en);
#else
int sprd_iommu_notifier_call_chain(void *data)
{
	return -ENODEV;
}

void sprd_iommu_pool_show(struct device *dev)
{
	return -ENODEV;
}

void sprd_iommu_reg_dump(struct device *dev)
{
	return -ENODEV;
}

void sprd_iommu_reg_show(struct device *dev)
{
	return -ENODEV;
}

static inline int sprd_iommu_attach_device(struct device *dev)
{
	return -ENODEV;
}

static inline int sprd_iommu_map(struct device *dev,
					struct sprd_iommu_map_data *data)
{
	return -ENODEV;
}

static inline int sprd_iommu_unmap(struct device *dev,
			struct sprd_iommu_unmap_data *data)
{
	return -ENODEV;
}

static inline int sprd_iommu_unmap_orphaned(struct sprd_iommu_unmap_data *data)
{
	return -ENODEV;
}

static inline int sprd_iommu_restore(struct device *dev)
{
	return -ENODEV;
}
#endif

struct sprd_iommu_ops {
	int (*init)(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data);
	int (*exit)(struct sprd_iommu_dev *dev);
	unsigned long (*iova_alloc)(struct sprd_iommu_dev *dev, size_t iova_length, struct sprd_iommu_map_data *);
	void (*iova_free)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
	int (*iova_map)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct sg_table *table, struct sprd_iommu_map_data *);
	int (*iova_unmap)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
	int (*backup)(struct sprd_iommu_dev *dev);
	int (*restore)(struct sprd_iommu_dev *dev);
	int (*set_bypass)(struct sprd_iommu_dev *dev, bool vaor_bp_en);
	void (*enable)(struct sprd_iommu_dev *dev);
	void (*disable)(struct sprd_iommu_dev *dev);
	int (*dump)(struct sprd_iommu_dev *dev, void *data);
	void (*open)(struct sprd_iommu_dev *dev);
	void (*release)(struct sprd_iommu_dev *dev);
	void (*pgt_show)(struct sprd_iommu_dev *dev);
	int (*iova_unmap_orphaned)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
};

enum IOMMU_ID {
	/*for sharkl2 iommu*/
	IOMMU_EX_VSP,
	IOMMU_EX_DCAM,
	IOMMU_EX_CPP,
	IOMMU_EX_GSP,
	IOMMU_EX_JPG,
	IOMMU_EX_DISP,
	IOMMU_EX_ISP,
	IOMMU_EX_NEWISP,
	IOMMU_EX_FD,
	IOMMU_EX_NPU,
	IOMMU_EX_EPP,
	IOMMU_EX_EDP,
	IOMMU_EX_IDMA,
	IOMMU_EX_VDMA,
	/*for sharkl5pro*/
	IOMMU_VAU_VSP,
	IOMMU_VAU_VSP1,
	IOMMU_VAU_VSP2,
	IOMMU_VAU_DCAM,
	IOMMU_VAU_DCAM1,
	IOMMU_VAU_CPP,
	IOMMU_VAU_GSP,
	IOMMU_VAU_GSP1,
	IOMMU_VAU_JPG,
	IOMMU_VAU_DISP,
	IOMMU_VAU_DISP1,
	IOMMU_VAU_ISP,
	IOMMU_VAU_FD,
	IOMMU_VAU_NPU,
	IOMMU_VAU_EPP,
	IOMMU_VAU_EDP,
	IOMMU_VAU_IDMA,
	IOMMU_VAU_VDMA,
	IOMMU_MAX,
};
extern struct sprd_iommu_ops sprd_iommuex_hw_ops;
extern struct sprd_iommu_ops sprd_iommuvau_hw_ops;

#define IOMMU_TAG "[iommu]"

#define IOMMU_DEBUG(fmt, ...) \
	pr_debug(IOMMU_TAG  " %s()-" pr_fmt(fmt), __func__, ##__VA_ARGS__)

#define IOMMU_ERR(fmt, ...) \
	pr_err(IOMMU_TAG  " %s()-" pr_fmt(fmt), __func__, ##__VA_ARGS__)

#define IOMMU_INFO(fmt, ...) \
	pr_info(IOMMU_TAG  "%s()-" pr_fmt(fmt), __func__, ##__VA_ARGS__)

#define IOMMU_WARN(fmt, ...) \
	pr_warn(IOMMU_TAG  "%s()-" pr_fmt(fmt), __func__, ##__VA_ARGS__)


#endif
