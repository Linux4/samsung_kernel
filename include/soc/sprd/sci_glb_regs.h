/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SCI_GLB_REGS_H__
#define __SCI_GLB_REGS_H__

#ifndef REG_GLB_CLR
#define REG_GLB_CLR(A)                  ( A + 0x2000 )
#endif
#ifndef REG_GLB_SET
#define REG_GLB_SET(A)                  ( A + 0x1000 )
#endif

#ifndef __ASSEMBLER__

extern unsigned long sprd_adi_base;
extern unsigned long sprd_adi_phys;
#define SPRD_ADI_BASE		sprd_adi_base
#define SPRD_ADI_PHYS		sprd_adi_phys

#include "iomap.h"

#define REGS_AP_AHB_BASE	SPRD_AHB_BASE
#define REGS_AP_APB_BASE	SPRD_APBREG_BASE
#define REGS_AON_APB_BASE	SPRD_AONAPB_BASE
#define REGS_GPU_APB_BASE	SPRD_GPUAPB_BASE
#define REGS_MM_AHB_BASE	SPRD_MMAHB_BASE
#define REGS_PMU_APB_BASE	SPRD_PMU_BASE
#define REGS_AON_CLK_BASE	SPRD_AONCKG_BASE
#define REGS_AON_CLK_CORE_BASE	SPRD_AONCKG_BASE
//#define REGS_AON_CLK_BASE	SPRD_AON_CLK_CORE_BASE
#define REGS_AP_CLK_BASE	SPRD_APBCKG_BASE
#define REGS_AP_CLK_CORE_BASE	SPRD_APBCKG_BASE
#define REGS_GPU_CLK_BASE	SPRD_GPUCKG_BASE
#define REGS_MM_CLK_BASE	SPRD_MMCKG_BASE
#define REGS_PUB_APB_BASE	SPRD_PUB_BASE
#define REGS_GLB_BASE		SPRD_AONAPB_BASE
/* XXX: please check it! */
#define REGS_AHB_BASE		(SPRD_AHB_BASE  + 0x200)
#define SPRD_MISC_BASE		SPRD_ADI_BASE

#define ANA_PWM_BASE		(SPRD_ADISLAVE_BASE + 0x20 )
#define ANA_WDG_BASE		(SPRD_ADISLAVE_BASE + 0x40 )
#define ANA_RTC_BASE		(SPRD_ADISLAVE_BASE + 0x80 )
#define ANA_EIC_BASE		(SPRD_ADISLAVE_BASE + 0x100 )
#define ANA_PIN_BASE		(SPRD_ADISLAVE_BASE + 0x180 )
#define ANA_THM_BASE		(SPRD_ADISLAVE_BASE + 0x280 )
#define ADC_BASE		(SPRD_ADISLAVE_BASE + 0x300 )
#define ANA_CTL_INT_BASE	(SPRD_ADISLAVE_BASE + 0x380 )
#define ANA_BTLC_INT_BASE	(SPRD_ADISLAVE_BASE + 0x3c0 )
#define ANA_AUDIFA_INT_BASE	(SPRD_ADISLAVE_BASE + 0x400 )
#define ANA_GPIO_INT_BASE	(SPRD_ADISLAVE_BASE + 0x480 )
#define ANA_FPU_INT_BASE	(SPRD_ADISLAVE_BASE + 0x500 )
#define ANA_AUDCFGA_INT_BASE	(SPRD_ADISLAVE_BASE + 0x600 )
#define ANA_HDT_INT_BASE	(SPRD_ADISLAVE_BASE + 0x700 )
#define ANA_CTL_GLB_BASE	(SPRD_ADISLAVE_BASE + 0x800 )
#define REGS_ADC_BASE		(REGS_ADISLAVE_BASE + 0x300)
#define ANA_REGS_GLB_BASE	(REGS_ADISLAVE_BASE + 0x800)

#ifdef CONFIG_64BIT
#define SPRD_DEV_P2V(paddr)	(paddr + io_addr_sprd.base)
#define SPRD_DEV_V2P(vaddr)	(vaddr - io_addr_sprd.base)
#else
#define in_range(b, first, len)	\
	((b) >= (first) && (b) <= (first) + (len) - 1)
#define to_range(b, first, base) \
	( (b) - (first) + (base) )
static inline unsigned long SPRD_DEV_P2V(paddr)
{
	struct iomap_sprd *mapp;
	int length = sizeof(struct iotable_sprd)/sizeof(struct iomap_sprd);
	for (mapp = (struct iomap_sprd*) &io_addr_sprd;
		length--; mapp++) {
		if (in_range(paddr, mapp->paddr, mapp->length)
				&& mapp->paddr && mapp->length)
			return to_range(paddr, mapp->paddr, mapp->vaddr);
	}
	return ~0;
}

static inline unsigned long SPRD_DEV_V2P(vaddr)
{
	struct iomap_sprd *mapp;
	int length = sizeof(struct iotable_sprd)/sizeof(struct iomap_sprd);
	for (mapp = (struct iomap_sprd*) &io_addr_sprd;
		length--; mapp++) {
		if (in_range(vaddr, mapp->vaddr, mapp->length)
				&& mapp->vaddr && mapp->length)
			return to_range(vaddr, mapp->vaddr, mapp->paddr);
	}
	return ~0;
}

#endif

#define SCI_ADDR(x, y)		\
	(phys_addr_t)((phys_addr_t)(x) + (phys_addr_t)(y))

#endif

#if defined(CONFIG_ARCH_SC8825)

#include "__regs_ahb.h"
#include "__regs_ana_glb2.h"
#include "__regs_ana_glb.h"
#include "__regs_emc.h"
#include "__regs_glb.h"

#elif defined(CONFIG_ARCH_SCX20)

#include "./chip_x20/__regs_ada_apb_rf.h"
//#include "./chip_x20/__regs_ana_apb_if.h"
#if defined(CONFIG_ADIE_SC2723S) || defined(CONFIG_ADIE_SC2723)
#include "./chip_x20/__regs_ana_sc2723_glb.h"
#else
#include "./chip_x20/__regs_ana_sc2713s_glb.h"
#endif
#include "./chip_x20/__regs_aon_apb.h"
#include "./chip_x20/__regs_ap_ahb.h"
#include "./chip_x20/__regs_ap_apb.h"
#include "./chip_x20/__regs_avs_apb.h"
#include "./chip_x20/__regs_gpu_apb_rf.h"
#include "./chip_x20/__regs_mm_ahb_rf.h"
#include "./chip_x20/__regs_pmu_apb.h"
#include "./chip_x20/__regs_pub_apb.h"
#include "./chip_x20/__regs_mm_clk.h"
#include "./chip_x20/__regs_aon_clk.h"
#include "./chip_x20/__regs_ap_clk.h"
#define REG_AON_APB_CHIP_ID	REG_AON_APB_AON_CHIP_ID

#elif defined(CONFIG_ARCH_SCX30G)

#include "./chip_x30g/__regs_ada_apb_rf.h"

#if defined(CONFIG_ADIE_SC2723S) || defined(CONFIG_ADIE_SC2723)
#include "./chip_x30g/__regs_ana_sc2723_glb.h"
#else
#include "./chip_x30g/__regs_ana_sc2713s_glb.h"
#endif

#if defined(CONFIG_ARCH_SCX30G2)
#if defined(CONFIG_ARCH_SCX30G3)
#include "./chip_x30g/__regs_aon_apb_tshark3.h"
#include "./chip_x30g/__regs_pmu_apb_tshark3.h"
#include "./chip_x30g/__regs_crypto_apb.h"
#else
#include "./chip_x30g/__regs_aon_apb_tshark2.h"
#include "./chip_x30g/__regs_pmu_apb_tshark2.h"
#endif 
#include "./chip_x30g/__regs_mm_ahb_rf_tshark2.h"
#include "./chip_x30g/__regs_codec_ahb.h"
#else
#include "./chip_x30g/__regs_aon_apb.h"
#include "./chip_x30g/__regs_mm_ahb_rf.h"
#include "./chip_x30g/__regs_pmu_apb.h"
#endif
#include "./chip_x30g/__regs_ap_ahb.h"
#include "./chip_x30g/__regs_ap_apb.h"
#include "./chip_x30g/__regs_avs_apb.h"
#include "./chip_x30g/__regs_gpu_apb_rf.h"
#include "./chip_x30g/__regs_pub_apb.h"
#include "./chip_x30g/__regs_mm_clk.h"
#include "./chip_x30g/__regs_aon_clk.h"
#include "./chip_x30g/__regs_ap_clk.h"
#define REG_AON_APB_CHIP_ID	REG_AON_APB_AON_CHIP_ID


#elif defined(CONFIG_ARCH_SCX15)
#define REGS_ANA_APB_IF_BASE	ANA_REGS_GLB_BASE
#include "./chip_x15/__regs_ana_apb_if.h"
#include "./chip_x15/__regs_aon_apb.h"
#include "./chip_x15/__regs_ap_ahb.h"
#include "./chip_x15/__regs_ap_apb.h"
#include "./chip_x15/__regs_cp_apb_rf.h"
#include "./chip_x15/__regs_wcdma_ahb_rf.h"
#include "./chip_x15/__regs_mm_ahb_rf.h"
#include "./chip_x15/__regs_pmu_apb.h"
#include "./chip_x15/__regs_pub_apb.h"
#include "./chip_x15/__regs_avs_apb.h"
#include "./chip_x15/__regs_aon_clk.h"
#include "./chip_x15/__regs_ap_clk.h"
#include "./chip_x15/__regs_mm_clk.h"
#include "./chip_x15/__regs_gpu_apb.h"
#include "./chip_x15/__regs_gpu_clk.h"
#ifndef BIT_GPIO_EB
#define BIT_GPIO_EB	BIT_AON_GPIO_EB
#endif
#define ANA_REG_GLB_LDO_SLP_CTRL0	ANA_REG_GLB_PWR_SLP_CTRL0
#define ANA_REG_GLB_LDO_SLP_CTRL1	ANA_REG_GLB_PWR_SLP_CTRL1
#define ANA_REG_GLB_LDO_SLP_CTRL2	ANA_REG_GLB_PWR_SLP_CTRL2
#define ANA_REG_GLB_LDO_SLP_CTRL3	ANA_REG_GLB_PWR_SLP_CTRL3
#define REG_AON_APB_CHIP_ID	REG_AON_APB_AON_CHIP_ID

#elif defined(CONFIG_ARCH_SCX35L) 
	#if defined (CONFIG_ARCH_SCX35L64)
		#include "./chip_x35l64/__regs_ana_sc2723_glb.h"
		#include "./chip_x35l64/__regs_ada_apb_rf.h"
		#include "./chip_x35l64/__regs_aon_apb_rf.h"
		#include "./chip_x35l64/__regs_aon_clk.h"
		#include "./chip_x35l64/__regs_ap_ahb_rf.h"
		#include "./chip_x35l64/__regs_ap_clk.h"
		#include "./chip_x35l64/__regs_ap_apb.h"
		#include "./chip_x35l64/__regs_gpu_apb_rf.h"
		#include "./chip_x35l64/__regs_mm_ahb_rf.h"
		#include "./chip_x35l64/__regs_mm_clk.h"
		#include "./chip_x35l64/__regs_pmu_apb_rf.h"
		#include "./chip_x35l64/__regs_pub_apb.h"
	#elif defined (CONFIG_ARCH_SCX35LT8)
		#include "./chip_x35lt8/__regs_ana_sc2723_glb.h"
		#include "./chip_x35lt8/__regs_ada_apb_rf.h"
		#include "./chip_x35lt8/__regs_aon_apb_rf.h"
		#include "./chip_x35lt8/__regs_aon_clk.h"
		#include "./chip_x35lt8/__regs_ap_ahb_rf.h"
		#include "./chip_x35lt8/__regs_ap_clk.h"
		#include "./chip_x35lt8/__regs_ap_apb.h"
		#include "./chip_x35lt8/__regs_codec_ahb.h"
		#include "./chip_x35lt8/__regs_gpu_apb_rf.h"
		#include "./chip_x35lt8/__regs_mm_ahb_rf.h"
		#include "./chip_x35lt8/__regs_mm_clk.h"
		#include "./chip_x35lt8/__regs_pmu_apb_rf.h"
		#include "./chip_x35lt8/__regs_pub_apb.h"

#define REG_AON_APB_CHIP_ID0	REG_AON_APB_PROJ_ID_0
#define REG_AON_APB_CHIP_ID1	REG_AON_APB_PROJ_ID_1

	#elif defined(CONFIG_ARCH_SCX20L)
		#include "./chip_x20l/__regs_ana_sc2723_glb.h"
		#include "./chip_x20l/__regs_ada_apb_rf.h"
		#include "./chip_x20l/__regs_aon_apb_rf.h"
		#include "./chip_x20l/__regs_aon_clk.h"
		#include "./chip_x20l/__regs_ap_ahb_rf.h"
		#include "./chip_x20l/__regs_ap_clk.h"
		#include "./chip_x20l/__regs_ap_apb.h"
		#include "./chip_x20l/__regs_gpu_apb_rf.h"
		#include "./chip_x20l/__regs_mm_ahb_rf.h"
		#include "./chip_x20l/__regs_mm_clk.h"
		#include "./chip_x20l/__regs_pmu_apb_rf.h"
		#include "./chip_x20l/__regs_pub_apb.h"
		#include "./chip_x20l/__regs_arm7_ahb_rf.h"
	#else
		#include "./chip_x35l/__regs_ana_sc2723_glb.h"
		#include "./chip_x35l/__regs_ada_apb_rf.h"
		#include "./chip_x35l/__regs_aon_apb_rf.h"
		#include "./chip_x35l/__regs_aon_clk.h"
		#include "./chip_x35l/__regs_ap_ahb_rf.h"
		#include "./chip_x35l/__regs_ap_clk.h"
		#include "./chip_x35l/__regs_ap_apb.h"
		#include "./chip_x35l/__regs_gpu_apb_rf.h"
		#include "./chip_x35l/__regs_mm_ahb_rf.h"
		#include "./chip_x35l/__regs_mm_clk.h"
		#include "./chip_x35l/__regs_pmu_apb_rf.h"
		#include "./chip_x35l/__regs_pub_apb.h"
	#if defined(CONFIG_MACH_SP9830I)
		#include "./chip_x35l/__regs_codec_ahb.h"
	#endif
	#endif
#define REG_AON_APB_CHIP_ID    REG_AON_APB_AON_CHIP_ID

#elif defined(CONFIG_ARCH_SCX35)

#if defined(CONFIG_ADIE_SC2713S)
#include "./chip_x35/__regs_ana_sc2713s_glb.h"
#else
#include "./chip_x35/__regs_ana_glb.h"
#endif
#include "./chip_x35/__regs_aon_apb.h"
#include "./chip_x35/__regs_aon_clk.h"
#include "./chip_x35/__regs_ap_ahb.h"
#include "./chip_x35/__regs_ap_apb.h"
#include "./chip_x35/__regs_ap_clk.h"
#include "./chip_x35/__regs_gpu_apb.h"
#include "./chip_x35/__regs_gpu_clk.h"
#include "./chip_x35/__regs_mm_ahb.h"
#include "./chip_x35/__regs_mm_clk.h"
#include "./chip_x35/__regs_pmu_apb.h"
#include "./chip_x35/__regs_pub_apb.h"


#else

#error "Unknown architecture specification"

#endif

#endif

