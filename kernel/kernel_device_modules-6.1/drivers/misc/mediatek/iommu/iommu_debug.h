/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#ifndef IOMMU_DEBUG_H
#define IOMMU_DEBUG_H

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "../../../iommu/mtk_iommu.h"
#include "../../../iommu/arm/arm-smmu-v3/mtk-smmu-v3.h"

#define DEFINE_PROC_ATTRIBUTE(__fops, __get, __set, __fmt)		  \
static int __fops ## _open(struct inode *inode, struct file *file)	  \
{									  \
	struct inode local_inode = *inode;				  \
									  \
	local_inode.i_private = pde_data(inode);			  \
	__simple_attr_check_format(__fmt, 0ull);			  \
	return simple_attr_open(&local_inode, file, __get, __set, __fmt); \
}									  \
static const struct proc_ops __fops = {					  \
	.proc_open	 = __fops ## _open,				  \
	.proc_release = simple_attr_release,				  \
	.proc_read	 = simple_attr_read,				  \
	.proc_write	 = simple_attr_write,				  \
	.proc_lseek	 = generic_file_llseek,				  \
}

enum iommu_event_type {
	IOMMU_ALLOC = 0,
	IOMMU_FREE,
	IOMMU_MAP,
	IOMMU_UNMAP,
	IOMMU_SYNC,
	IOMMU_UNSYNC,
	IOMMU_SUSPEND,
	IOMMU_RESUME,
	IOMMU_POWER_ON,
	IOMMU_POWER_OFF,
	IOMMU_EVENT_MAX
};

typedef int (*mtk_iommu_fault_callback_t)(int port,
	dma_addr_t mva, void *cb_data);

void report_custom_iommu_fault(
	u64 fault_iova, u64 fault_pa, u32 fault_id,
	enum mtk_iommu_type type, int id);

void report_iommu_mau_fault(
	u32 assert_id, u32 falut_id, char *port_name,
	u32 assert_addr, u32 assert_b32);

int mtk_iommu_register_fault_callback(int port,
	mtk_iommu_fault_callback_t fn,
	void *cb_data, bool is_vpu);

/* port: comes from "include/dt-binding/memort/mtxxx-larb-port.h" */
int mtk_iommu_unregister_fault_callback(int port, bool is_vpu);
void mtk_iova_map(u64 tab_id, u64 iova, size_t size);
void mtk_iova_unmap(u64 tab_id, u64 iova, size_t size);
void mtk_iova_map_dump(u64 iova, u64 tab_id);
void mtk_iova_dump(u64 iova, u64 tab_id);
void mtk_iova_trace_dump(u64 iova);
void mtk_iommu_tlb_sync_trace(u64 iova, size_t size, int iommu_ids);
void mtk_iommu_pm_trace(int event, int iommu_id, int pd_sta,
	unsigned long flags, struct device *dev);

void mtk_iommu_debug_reset(void);
enum peri_iommu get_peri_iommu_id(u32 bus_id);
char *peri_tf_analyse(enum peri_iommu iommu_id, u32 fault_id);
char *mtk_iommu_get_port_name(enum mtk_iommu_type type, int id, int tf_id);
const struct mau_config_info *mtk_iommu_get_mau_config(
	enum mtk_iommu_type type, int id,
	unsigned int slave, unsigned int mau);
int mtk_iommu_set_ops(const struct mtk_iommu_ops *ops);
int mtk_iommu_update_pm_status(u32 type, u32 id, bool pm_sta);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
int mtk_smmu_set_ops(const struct mtk_smmu_ops *ops);

void report_custom_smmu_fault(u64 fault_iova, u64 fault_pa,
			      u32 fault_id, u32 smmu_id);

void mtk_smmu_wpreg_dump(struct seq_file *s, u32 smmu_type);
void mtk_smmu_ste_cd_dump(struct seq_file *s, u32 smmu_type);
void mtk_smmu_ste_cd_info_dump(struct seq_file *s, u32 smmu_type, u32 sid);
void mtk_smmu_pgtable_dump(struct seq_file *s, u32 smmu_type, bool dump_rawdata);
void mtk_smmu_pgtable_ops_dump(struct seq_file *s, struct io_pgtable_ops *ops);
u64 mtk_smmu_iova_to_iopte(struct io_pgtable_ops *ops, u64 iova);
#else /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
static inline int mtk_smmu_set_ops(const struct mtk_smmu_ops *ops)
{
	return 0;
}

static inline void report_custom_smmu_fault(u64 fault_iova, u64 fault_pa,
					    u32 fault_id, u32 smmu_id)
{
}

static inline void mtk_smmu_wpreg_dump(struct seq_file *s, u32 smmu_type)
{
}

static inline void mtk_smmu_ste_cd_dump(struct seq_file *s, u32 smmu_type)
{
}

static inline void mtk_smmu_ste_cd_info_dump(struct seq_file *s, u32 smmu_type, u32 sid)
{
}

static inline void mtk_smmu_pgtable_dump(struct seq_file *s, u32 smmu_type, bool dump_rawdata)
{
}

static inline void mtk_smmu_pgtable_ops_dump(struct seq_file *s, struct io_pgtable_ops *ops)
{
}

static inline u64 mtk_smmu_iova_to_iopte(struct io_pgtable_ops *ops, u64 iova)
{
	return 0;
}
#endif /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
#endif /* IOMMU_DEBUG_H */
