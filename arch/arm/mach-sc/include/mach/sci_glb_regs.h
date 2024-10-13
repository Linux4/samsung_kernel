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
 *
 *************************************************
 * Automatically generated C header: do not edit *
 *************************************************
 */

#ifndef __SCI_GLB_REGS_H__
#define __SCI_GLB_REGS_H__

#ifndef REG_GLB_CLR
#define REG_GLB_CLR(A)                  ( A + 0x2000 )
#endif
#ifndef REG_GLB_SET
#define REG_GLB_SET(A)                  ( A + 0x1000 )
#endif

#if defined(CONFIG_ARCH_SC8825)

#include "__regs_ahb.h"
#include "__regs_ana_glb2.h"
#include "__regs_ana_glb.h"
#include "__regs_emc.h"
#include "__regs_glb.h"

#elif defined(CONFIG_ARCH_SCX30G)

#include "./chip_x30g/__regs_ada_apb_rf.h"
//#include "./chip_x30g/__regs_ana_apb_if.h"
#if defined(CONFIG_ADIE_SC2723S) || defined(CONFIG_ADIE_SC2723)
#include "./chip_x30g/__regs_ana_sc2723_glb.h"
#else
#include "./chip_x30g/__regs_ana_sc2713s_glb.h"
#endif
#include "./chip_x30g/__regs_aon_apb.h"
#include "./chip_x30g/__regs_ap_ahb.h"
#include "./chip_x30g/__regs_ap_apb.h"
#include "./chip_x30g/__regs_avs_apb.h"
#include "./chip_x30g/__regs_gpu_apb_rf.h"
#include "./chip_x30g/__regs_mm_ahb_rf.h"
#include "./chip_x30g/__regs_pmu_apb.h"
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

