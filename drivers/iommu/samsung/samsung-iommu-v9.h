/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 */

#ifndef __SAMSUNG_IOMMU_V9_H
#define __SAMSUNG_IOMMU_V9_H

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/iommu.h>

#define SYSMMU_VM_OFFSET	0x1000
#define SYSMMU_MASK_VMID	0x1
#define PMMU_MAX_NUM		8

#define REG_MMU_CTRL				0x0000
#define REG_MMU_STATUS				0x0008
#define REG_MMU_VERSION				0x0034

#define REG_MMU_STREAM_CFG(n)			(0x2000 + ((n) * 0x10))
#define REG_MMU_STREAM_MATCH_CFG(n)		(0x2000 + ((n) * 0x10) + 0x4)
#define REG_MMU_STREAM_MATCH_SID_VALUE(n)	(0x2000 + ((n) * 0x10) + 0x8)
#define REG_MMU_STREAM_MATCH_SID_MASK(n)	(0x2000 + ((n) * 0x10) + 0xC)

#define REG_MMU_PMMU_INDICATOR			0x2FFC
#define REG_MMU_PMMU_INFO			0x3000
#define REG_MMU_SWALKER_INFO			0x3004

#define REG_MMU_CTRL_VM				0x8000
#define REG_MMU_CONTEXT0_CFG_FLPT_BASE_VM	0x8404
#define REG_MMU_CONTEXT0_CFG_ATTRIBUTE_VM	0x8408

#define MMU_VERSION_MAJOR(val)			((val) >> 12)
#define MMU_VERSION_MINOR(val)			(((val) >> 8) & 0xF)
#define MMU_VERSION_REVISION(val)		((val) & 0xFF)
#define MMU_VERSION_RAW(reg)			(((reg) >> 16) & 0xFFFF)
#define MMU_VERSION(x, y, z)			((z) | ((y) << 8) | ((z) << 12))
#define MMU_VM_ADDR(reg, idx)			((reg) + ((idx) * SYSMMU_VM_OFFSET))

#define CTRL_MMU_ENABLE			BIT(0)
#define CTRL_INT_ENABLE			BIT(2)
#define CTRL_FAULT_STALL_MODE		BIT(3)

#define MMU_STREAM_CFG_STLB_ID(val)		(((val) >> 24) & 0xFF)
#define MMU_STREAM_CFG_PTLB_ID(val)		(((val) >> 16) & 0xFF)

#define MMU_SET_PMMU_INDICATOR(val)		((val) & 0xF)

#define MMU_CONTEXT0_CFG_FLPT_BASE_PPN(reg)	((reg) & 0xFFFFFF)

typedef u64 sysmmu_iova_t;
typedef u32 sysmmu_pte_t;

#define CFG_QOS_OVRRIDE			BIT(11)
#define CFG_QOS(n)			(((n) & 0xF) << 7)

#define SECT_ORDER	20
#define LPAGE_ORDER	16
#define SPAGE_ORDER	12

#define SECT_SIZE	BIT(SECT_ORDER)
#define LPAGE_SIZE	BIT(LPAGE_ORDER)
#define SPAGE_SIZE	BIT(SPAGE_ORDER)

#define SECT_MASK		GENMASK(63, SECT_ORDER)
#define LPAGE_MASK		GENMASK(63, LPAGE_ORDER)
#define SPAGE_MASK		GENMASK(63, SPAGE_ORDER)

#define SECT_ENT_MASK		GENMASK(63, SECT_ORDER - PG_ENT_SHIFT)
#define LPAGE_ENT_MASK		GENMASK(63, LPAGE_ORDER - PG_ENT_SHIFT)
#define SPAGE_ENT_MASK		GENMASK(63, SPAGE_ORDER - PG_ENT_SHIFT)

#define SPAGES_PER_LPAGE	(LPAGE_SIZE / SPAGE_SIZE)

#define NUM_LV1ENTRIES		65536
#define NUM_LV2ENTRIES		(SECT_SIZE / SPAGE_SIZE)
#define LV2TABLE_SIZE		(NUM_LV2ENTRIES * sizeof(sysmmu_pte_t))

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & ~SECT_MASK) >> SPAGE_ORDER)

#define FLPD_FLAG_MASK	7
#define SLPD_FLAG_MASK	3
#define UNMAPPED_FLAG	0
#define SLPD_FLAG	1
#define SECT_FLAG	2
#define LPAGE_FLAG	1
#define SPAGE_FLAG	2

#define PG_ENT_SHIFT	4

#define lv1ent_unmapped(sent)	((*(sent) & FLPD_FLAG_MASK) == UNMAPPED_FLAG)
#define lv1ent_page(sent)	((*(sent) & FLPD_FLAG_MASK) == SLPD_FLAG)
#define lv1ent_section(sent)	((*(sent) & FLPD_FLAG_MASK) == SECT_FLAG)
#define lv1ent_offset(iova)	((iova) >> SECT_ORDER)

#define lv2table_base(sent)	((phys_addr_t)(*(sent) & ~0x3F) << PG_ENT_SHIFT)
#define lv2ent_unmapped(pent)	((*(pent) & SLPD_FLAG_MASK) == UNMAPPED_FLAG)
#define lv2ent_small(pent)	((*(pent) & SLPD_FLAG_MASK) == SPAGE_FLAG)
#define lv2ent_large(pent)	((*(pent) & SLPD_FLAG_MASK) == LPAGE_FLAG)
#define lv2ent_offset(iova)	(((iova) & ~SECT_MASK) >> SPAGE_ORDER)

#define	PGBASE_TO_PHYS(pgent)	((phys_addr_t)(pgent) << PG_ENT_SHIFT)
#define ENT_TO_PHYS(ent)	((phys_addr_t)(*(ent)))
#define section_phys(sent)	PGBASE_TO_PHYS(ENT_TO_PHYS(sent) & SECT_ENT_MASK)
#define section_offs(iova)	((iova) & (SECT_SIZE - 1))
#define lpage_phys(pent)	PGBASE_TO_PHYS(ENT_TO_PHYS(pent) & LPAGE_ENT_MASK)
#define lpage_offs(iova)	((iova) & (LPAGE_SIZE - 1))
#define spage_phys(pent)	PGBASE_TO_PHYS(ENT_TO_PHYS(pent) & SPAGE_ENT_MASK)
#define spage_offs(iova)	((iova) & (SPAGE_SIZE - 1))

static inline sysmmu_pte_t *page_entry(sysmmu_pte_t *sent, sysmmu_iova_t iova)
{
	return (sysmmu_pte_t *)(phys_to_virt(lv2table_base(sent))) + lv2ent_offset(iova);
}

static inline sysmmu_pte_t *section_entry(sysmmu_pte_t *pgtable, sysmmu_iova_t iova)
{
	return pgtable + lv1ent_offset(iova);
}

#define SYSMMU_EVENT_MAX	2048
enum sysmmu_event_type {
	/* init value*/
	SYSMMU_EVENT_NONE,
	/* event for sysmmu drvdata */
	SYSMMU_EVENT_ENABLE,
	SYSMMU_EVENT_DISABLE,
	SYSMMU_EVENT_POWERON,
	SYSMMU_EVENT_POWEROFF,
	SYSMMU_EVENT_INVALIDATE_RANGE,
	SYSMMU_EVENT_INVALIDATE_ALL,
	SYSMMU_EVENT_IOTLB_SYNC,
	/* event for iommu domain */
	SYSMMU_EVENT_MAP,
	SYSMMU_EVENT_UNMAP,
};

struct sysmmu_log {
	unsigned long long time;
	enum sysmmu_event_type type;
	u64 start;
	u64 end;
};

struct samsung_iommu_log {
	atomic_t index;
	int len;
	struct sysmmu_log *log;
};

struct stream_config {
	unsigned int index;
	u32 cfg;
	u32 match_cfg;
	u32 match_id_value;
	u32 match_id_mask;
};

struct stream_props {
	int id_cnt;
	u32 default_cfg;
	struct stream_config *cfg;
};

struct sysmmu_drvdata {
	struct list_head list;
	struct iommu_device iommu;
	struct device *dev;
	struct iommu_group *group;
	void __iomem *sfrbase;
	struct clk *clk;
	phys_addr_t pgtable;
	spinlock_t lock; /* protect atomic update to H/W status */
	u32 version;
	u32 va_width;
	u32 vmid_mask;
	int max_vm;
	int num_pmmu;
	int *num_stream_table;
	int *num_ptlb;
	int qos;
	int attached_count;
	int rpm_count;
	int secure_irq;
	unsigned int secure_base;
	bool async_fault_mode;
	struct stream_props *props;
	struct samsung_iommu_log log;
};

struct sysmmu_clientdata {
	struct device *dev;
	struct sysmmu_drvdata **sysmmus;
	struct device_link **dev_link;
	int sysmmu_count;
};

irqreturn_t samsung_sysmmu_irq_thread(int irq, void *dev_id);
irqreturn_t samsung_sysmmu_irq(int irq, void *dev_id);

#endif
