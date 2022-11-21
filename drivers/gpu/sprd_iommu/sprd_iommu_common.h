#ifndef __SPRD_IOMMU_COMMON_H__
#define __SPRD_IOMMU_COMMON_H__

#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/sprd_iommu.h>
#include <linux/scatterlist.h>
#include <linux/of.h>

#define OFFSET_CFG                     0x4
#define OFFSET_START_MB_ADDR_LO        0x8
#define OFFSET_START_MB_ADDR_HI        0xC
#define OFFSET_RR_DEST_LO              0x10
#define OFFSET_RR_DEST_HI              0x14
#define OFFSET_OFR_ADDR_LO_R           0x100
#define OFFSET_OFR_ADDR_HI_R           0x104
#define OFFSET_TRANS_CNT_R             0x108
#define OFFSET_PT_MISS_CNT_R           0x10C
#define OFFSET_TLB_MISS_CNT_R          0x110
#define OFFSET_OFR_ADDR_LO_W           0x114
#define OFFSET_OFR_ADDR_HI_W           0x118
#define OFFSET_TRANS_CNT_W             0x11C
#define OFFSET_PT_MISS_CNT_W           0x120
#define OFFSET_TLB_MISS_CNT_W          0x124
#define OFFSET_AXI_STAT                0x128
#define OFFSET_FSM_STAT                0x12C
#define OFFSET_PARAM                   0x3F8
#define OFFSET_REV                     0x3FC

#define AC(X, Y)  (X##Y)

/*common define*/
/*MMU_CTRL register*/
#define MMU_EN(_x_)                     ((AC(_x_, UL)) << 0 & (BIT(0)))
#define MMU_EN_MASK                     (BIT(0))
#define MMU_TLB_EN(_x_)                 ((AC(_x_, UL)) << 1 & (BIT(1)))
#define MMU_TLB_EN_MASK                 (BIT(1))

/*IOMMU rev1 define*/
#define MMU_V1_RAMCLK_DIV2_EN(_x_)      ((AC(_x_, UL)) << 2 & (BIT(2)))
#define MMU_V1_RAMCLK_DIV2_EN_MASK      (BIT(2))
#define MMU_START_MB_ADDR_MASK          (BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31))

/*IOMMU rev2 define*/
/*MMU_CFG register*/
#define MMU_RAMCLK_DIV2_EN(_x_)         ((AC(_x_, UL)) << 0 & (BIT(0)))
#define MMU_RAMCLK_DIV2_EN_MASK         (BIT(0))
#define MMU_TLB_RP_R(_x_)               ((AC(_x_, UL)) << 4 & (BIT(4)))
#define MMU_TLB_RP_R_MASK               (BIT(4))
#define MMU_TLB_RP_W(_x_)               ((AC(_x_, UL)) << 5 & (BIT(5)))
#define MMU_TLB_RP_W_MASK               (BIT(5))
#define MMU_RR_EN(_x_)                  ((AC(_x_, UL)) << 6 & (BIT(6)))
#define MMU_RR_EN_MASK                  (BIT(6))
#define MMU_MON_EN(_x_)                 ((AC(_x_, UL)) << 7 & (BIT(7)))
#define MMU_MON_EN_MASK                 (BIT(7))

/*MMU_START_MB_ADDR registers*/
#define MMU_START_MB_ADDR_LO_MASK       (BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31))
#define MMU_START_MB_ADDR_HI_MASK       (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7))

/*MMU_RR_DEST registers*/
#define MMU_RR_DEST_LO_MASK             (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31))
#define MMU_RR_DEST_HI_MASK             (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7))

/*MMU_OFR_ADDR_R registers*/
#define MMU_OFR_ADDR_LO_R_MASK          (0xFFFFFFFFUL)
#define MMU_OFR_ADDR_HI_R_MASK          (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7))

/*MMU CNT_R registers*/
#define MMU_TRANS_CNT_R_MASK            (0xFFFFFFFFUL)
#define MMU_PT_MISS_CNT_R_MASK          (0xFFFFFFFFUL)
#define MMU_TLB_MISS_CNT_R_MASK         (0xFFFFFFFFUL)

/*MMU_OFR_ADDR_W register*/
#define MMU_OFR_ADDR_LO_W_MASK          (0xFFFFFFFFUL)
#define MMU_OFR_ADDR_HI_W_MASK          (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7))

/*MMU CNT_W registers*/
#define MMU_TRANS_CNT_W_MASK            (0xFFFFFFFFUL)
#define MMU_PT_MISS_CNT_W_MASK          (0xFFFFFFFFUL)
#define MMU_TLB_MISS_CNT_W_MASK         (0xFFFFFFFFUL)

/*MMU_PARAM registers*/
#define MMU_PARAM_AXI_ABW_MASK          (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29))
#define MMU_PARAM_TLB_SIZE_MASK         (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23))
#define MMU_PARAM_PGAE_SIZE_MASK        (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12))
#define MMU_PARAM_PT_SIZE_MASK          (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4))

/*MMU_REV registers*/
#define MMU_MAJOR_REV_MASK              (BIT(4)|BIT(5)|BIT(6)|BIT(7))
#define MMU_MINOR_REV_MASK              (BIT(0)|BIT(1)|BIT(2)|BIT(3))


#define MMU_PAGE_SIZE	PAGE_SIZE /*0x1000  need check*/
/**
 * Page table index from address
 * Calculates the page table index from the given address
*/
#define MMU_PTE_OFFSET(address) (((address)>>12) & 0xFFFF)

/*64-bit virtual addr and 32bit phy addr in arm 64-bit of TSharkL.
  *So we use (sizeof(unsigned int)).
  */
#define MMU_IOVA_SIZE_TO_PTE(size)  ((sizeof(unsigned int))*(size)/MMU_PAGE_SIZE)

extern const struct of_device_id iommu_ids[];

void mmu_reg_write(unsigned long reg, unsigned long val, unsigned long msk);

int sprd_iommu_init(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data);

int sprd_iommu_exit(struct sprd_iommu_dev *dev);

unsigned long sprd_iommu_iova_alloc(struct sprd_iommu_dev *dev, size_t iova_length);

void sprd_iommu_iova_free(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);

int sprd_iommu_iova_map(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct sg_table *table);

int sprd_iommu_iova_unmap(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);

int sprd_iommu_backup(struct sprd_iommu_dev *dev);

int sprd_iommu_restore(struct sprd_iommu_dev *dev);

void sprd_iommu_disable(struct sprd_iommu_dev *dev);

void sprd_iommu_enable(struct sprd_iommu_dev *dev);

void sprd_iommu_reset_enable(struct sprd_iommu_dev *dev);

int sprd_iommu_dump(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);

#endif
