/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SPRD_IOMMU_API_H_
#define _SPRD_IOMMU_API_H_

#include "../inc/sprd_defs.h"
#include "../com/sprd_com.h"

/*Declares a handle to the iommu object*/
SPRD_DECLARE_HANDLE(sprd_iommu_hdl);

extern struct sprd_iommudrv_ops iommuex_drv_ops;
extern struct sprd_iommudrv_ops iommuvau_drv_ops;

enum sprd_iommu_ch_type {
	PF_CH_READ = 0x100,/*prefetch channel only support read*/
	PF_CH_WRITE,/* prefetch channel only support write*/
	FM_CH_RW,/*ullmode channel support write/read in one channel*/
	EX_CH_READ,/*channel only support read, only ISP use now*/
	EX_CH_WRITE,/*channel only support read, only ISP use now*/
	CH_TYPE_INVALID, /*unsupported channel type*/
};

struct sprd_iommu_init_param {
	enum IOMMU_ID iommu_id;
	ulong frc_reg_addr;/*force copy register address */
	ulong base_reg_addr;
	u32 pgt_size;
	ulong ctrl_reg_addr;

	ulong vpn_base_addr;/*fullmode virtual pool base address*/
	u32 vpn_range;
	u64 faultpage_addr;/* Enabel fault page function */
	unsigned long pagt_base_ddr;
	unsigned int pagt_ddr_size;

	/*for sharkl2/isharkl2*/
	u64 mini_ppn1;
	u64 ppn1_range;
	u64 mini_ppn2;
	u64 ppn2_range;
};

struct sprd_iommu_map_param {
	u64 start_virt_addr;
	u32 total_map_size;
	struct sg_table *sg_table;
	u32 sg_offset;
};


struct sprd_iommu_unmap_param {
	u64 start_virt_addr;
	u32 total_map_size;
	enum sprd_iommu_ch_type ch_type;
	u32 ch_id;
};

struct sprd_iommudrv_ops {
	u32 (*init)(struct sprd_iommu_init_param *, sprd_iommu_hdl);
	u32  (*uninit)(sprd_iommu_hdl);

	u32  (*map)(sprd_iommu_hdl, struct sprd_iommu_map_param *);
	u32  (*unmap)(sprd_iommu_hdl, struct sprd_iommu_unmap_param *);

	u32  (*enable)(sprd_iommu_hdl);
	u32  (*disable)(sprd_iommu_hdl);

	u32  (*suspend)(sprd_iommu_hdl);
	u32  (*resume)(sprd_iommu_hdl);
	u32  (*release)(sprd_iommu_hdl);

	u32  (*reset)(sprd_iommu_hdl, u32);
	u32  (*set_bypass)(sprd_iommu_hdl, bool);
	u32  (*virttophy)(sprd_iommu_hdl, u64, u64 *);

	u32  (*unmap_orphaned)(sprd_iommu_hdl, struct sprd_iommu_unmap_param *);
};


struct sprd_iommu_data {
	void *priv;
	struct sprd_iommudrv_ops *iommudrv_ops;
};

u32 sprd_iommudrv_init(struct sprd_iommu_init_param *init_param,
		  sprd_iommu_hdl *iommu_hdl);
u32 sprd_iommudrv_uninit(sprd_iommu_hdl iommu_hdl);
u32 sprd_iommudrv_map(sprd_iommu_hdl iommu_hdl,
		  struct sprd_iommu_map_param *map_param);
u32 sprd_iommudrv_unmap(sprd_iommu_hdl iommu_hdl,
		  struct sprd_iommu_unmap_param *unmap_param);
u32 sprd_iommudrv_unmap_orphaned(sprd_iommu_hdl iommu_hdl,
			struct sprd_iommu_unmap_param *unmap_param);
u32 sprd_iommudrv_enable(sprd_iommu_hdl iommu_hdl);
u32 sprd_iommudrv_disable(sprd_iommu_hdl iommu_hdl);
u32 sprd_iommudrv_suspend(sprd_iommu_hdl iommu_hdl);
u32 sprd_iommudrv_resume(sprd_iommu_hdl iommu_hdl);
u32 sprd_iommudrv_release(sprd_iommu_hdl iommu_hdl);
u32 sprd_iommudrv_reset(sprd_iommu_hdl iommu_hdl, u32 channel_num);
u32 sprd_iommudrv_set_bypass(sprd_iommu_hdl  iommu_hdl, bool vaor_bp_en);
u32 sprd_iommudrv_virt_to_phy(sprd_iommu_hdl iommu_hdl,
			  u64 virt_addr, u64 *dest_addr);
#endif  /*END OF : define  _SPRD_IOMMU_API_H_ */
