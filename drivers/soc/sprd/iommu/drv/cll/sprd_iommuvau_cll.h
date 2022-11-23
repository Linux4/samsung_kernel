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

#ifndef _SPRD_IOMMU_CLL_H_
#define _SPRD_IOMMU_CLL_H_

#include "../inc/sprd_defs.h"
#include "../com/sprd_com.h"
#include "../api/sprd_iommu_api.h"
#include "../hal/sprd_iommuvau_hal_register.h"

struct sprd_iommuvau_interrupt {
	u8 pa_out_range_r_en;
	u8 pa_out_range_w_en;
	u8 va_out_range_r_en;
	u8 va_out_range_w_en;
	u8 invalid_r_en;
	u8 invalid_w_en;
	u8 unsecure_r_en;
	u8 unsecure_w_en;
};

struct sprd_iommuvau_priv {
	ulong mmu_reg_addr;/*mmu register offset from master base addr*/
	u32 pgt_size;

	u8 va_out_bypass_en;/*va out of range bypass,1 default*/
	ulong vpn_base_addr;
	u32 vpn_range;
	ulong ppn_base_addr;/*pagetable base addr in ddr*/
	ulong default_addr;
	ulong mini_ppn1;
	ulong ppn1_range;
	ulong mini_ppn2;
	ulong ppn2_range;
	/*iommu reserved memory of pf page table*/
	unsigned long pagt_base_ddr;
	unsigned int pagt_ddr_size;
	unsigned long pagt_base_phy_ddr;

	u32 map_cnt;
	enum IOMMU_ID iommu_id;
};
#endif  /* _SPRD_IOMMU_CLL_H_ */
