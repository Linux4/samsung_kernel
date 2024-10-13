/*
***************************************************************************
*
*  Copyright (C) 2020 Advanced Micro Devices, Inc.  All rights reserved.
*
***************************************************************************/

#include "amdgpu.h"

#ifndef __AMDGPU_VANGOGH_REG_H__
#define __AMDGPU_VANGOGH_REG_H__

#define BG3D_PWRCTL_CTL1_OFFSET 			0x0
#define BG3D_PWRCTL_CTL2_OFFSET 			0x4
#define BG3D_PWRCTL_STATUS_OFFSET 			0x8
#define BG3D_PWRCTL_LOCK_OFFSET				0xc

#define BG3D_PWRCTL_CTL2_GPUPWRREQ			0x12

#define BG3D_PWRCTL_STATUS_LOCK_VALUE__SHIFT            0x18
#define BG3D_PWRCTL_STATUS_GPU_READY_MASK 		0x80000000L
#define BG3D_PWRCTL_STATUS_GPUPWRREQ_VALUE_MASK		0x40000000L
#define BG3D_PWRCTL_STATUS_LOCK_VALUE_MASK              0x3F000000L
#define BG3D_PWRCTL_STATUS_GPUPWRACK_MASK		0x00000002L

#define BG3D_PWRCTL_LOCK_GPU_READY_MASK 		0x80000000L
#define BG3D_PWRCTL_LOCK_GPU_RESET_MASK 		0x00000001L

#define RSMU_HARD_RESETB_GC_OFFSET			0x40008
#define SMC_SAFE_WAIT_US				1000
#define QUIESCE_TIMEOUT_WAIT_US				50000

/* GL2ACEM_RST offset changed at M2 EVT1 */
#if IS_ENABLED(CONFIG_SOC_S5E9945) && !IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
#define GL2ACEM_RST_OFFSET				0x33A00
#else
#define GL2ACEM_RST_OFFSET				0x3f100
#endif

/* DIDT/EDC */
#define DIDT_SQ_CTRL0					0x00000000
#define DIDT_SQ_CTRL1                                   0x00000001
#define DIDT_SQ_CTRL2                                   0x00000002
#define DIDT_SQ_STALL_CTRL                              0x00000004
#define DIDT_SQ_TUNING_CTRL                             0x00000005
#define DIDT_SQ_WEIGHT0_3                               0x00000010
#define DIDT_SQ_WEIGHT4_7                               0x00000011
#define DIDT_SQ_EDC_CTRL				0x00000014
#define DIDT_SQ_EDC_THRESHOLD                           0x00000015
#define DIDT_SQ_EDC_STALL_PATTERN_1_2                   0x00000016
#define DIDT_SQ_EDC_STALL_PATTERN_3_4                   0x00000017
#define DIDT_SQ_EDC_STALL_PATTERN_5_6                   0x00000018
#define DIDT_SQ_EDC_STALL_PATTERN_7                     0x00000019

#endif
