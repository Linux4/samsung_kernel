/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __GPUFREQ_REG_MT6989_H__
#define __GPUFREQ_REG_MT6989_H__

#include <linux/io.h>
#include <linux/bits.h>

/**************************************************
 * GPUFREQ Register Operation
 **************************************************/

/**************************************************
 * GPUFREQ Register Definition
 **************************************************/
#define MALI_BASE                              (0x13000000)
#define MALI_GPU_ID                            (g_mali_base + 0x000)               /* 0x13000000 */
#define MALI_SHADER_READY_LO                   (g_mali_base + 0x140)               /* 0x13000140 */
#define MALI_SHADER_READY_HI                   (g_mali_base + 0x144)               /* 0x13000144 */
#define MALI_TILER_READY_LO                    (g_mali_base + 0x150)               /* 0x13000150 */
#define MALI_TILER_READY_HI                    (g_mali_base + 0x154)               /* 0x13000154 */
#define MALI_L2_READY_LO                       (g_mali_base + 0x160)               /* 0x13000160 */
#define MALI_L2_READY_HI                       (g_mali_base + 0x164)               /* 0x13000164 */
#define MALI_L2_PWRON_LO                       (g_mali_base + 0x1A0)               /* 0x130001A0 */
#define MALI_L2_PWRON_HI                       (g_mali_base + 0x1A4)               /* 0x130001A4 */
#define MALI_L2_PWROFF_LO                      (g_mali_base + 0x1E0)               /* 0x130001E0 */
#define MALI_L2_PWROFF_HI                      (g_mali_base + 0x1E4)               /* 0x130001E4 */
#define MALI_SHADER_PWRTRANS_LO                (g_mali_base + 0x200)               /* 0x13000200 */
#define MALI_SHADER_PWRTRANS_HI                (g_mali_base + 0x204)               /* 0x13000204 */
#define MALI_TILER_PWRTRANS_LO                 (g_mali_base + 0x210)               /* 0x13000210 */
#define MALI_TILER_PWRTRANS_HI                 (g_mali_base + 0x214)               /* 0x13000214 */
#define MALI_L2_PWRTRANS_LO                    (g_mali_base + 0x220)               /* 0x13000220 */
#define MALI_L2_PWRTRANS_HI                    (g_mali_base + 0x224)               /* 0x13000224 */
#define MALI_GPU_IRQ_CLEAR                     (g_mali_base + 0x024)               /* 0x13000024 */
#define MALI_GPU_IRQ_MASK                      (g_mali_base + 0x028)               /* 0x13000028 */
#define MALI_GPU_IRQ_STATUS                    (g_mali_base + 0x02C)               /* 0x1300002C */

#define MFG_TOP_CFG_BASE                       (0x13FBF000)
#define MFG_CG_CLR                             (g_mfg_top_base + 0x008)            /* 0x13FBF008 */
#define MFG_DCM_CON_0                          (g_mfg_top_base + 0x010)            /* 0x13FBF010 */
#define MFG_GLOBAL_CON                         (g_mfg_top_base + 0x0B0)            /* 0x13FBF0B0 */
#define MFG_ASYNC_CON                          (g_mfg_top_base + 0x020)            /* 0x13FBF020 */
#define MFG_ASYNC_CON3                         (g_mfg_top_base + 0x02C)            /* 0x13FBF02C */
#define MFG_ASYNC_CON4                         (g_mfg_top_base + 0x1B0)            /* 0x13FBF1B0 */
#define MFG_DEBUG_SEL                          (g_mfg_top_base + 0x170)            /* 0x13FBF170 */
#define MFG_DEBUG_TOP                          (g_mfg_top_base + 0x178)            /* 0x13FBF178 */
#define MFG_QCHANNEL_CON                       (g_mfg_top_base + 0x0B4)            /* 0x13FBF0B4 */
#define MFG_DUMMY_REG                          (g_mfg_top_base + 0x500)            /* 0x13FBF500 */
#define MFG_TIMESTAMP                          (g_mfg_top_base + 0x130)            /* 0x13FBF130 */
#define MFG_DEFAULT_DELSEL_00                  (g_mfg_top_base + 0xC80)            /* 0x13FBFC80 */
#define MFG_ACTIVE_POWER_CON_CG_06             (g_mfg_top_base + 0x100)            /* 0x13FBF100 */
#define MFG_ACTIVE_POWER_CON_ST0_06            (g_mfg_top_base + 0x120)            /* 0x13FBF120 */
#define MFG_ACTIVE_POWER_CON_ST1_06            (g_mfg_top_base + 0x140)            /* 0x13FBF140 */
#define MFG_ACTIVE_POWER_CON_ST3_06            (g_mfg_top_base + 0x0A0)            /* 0x13FBF0A0 */
#define MFG_ACTIVE_POWER_CON_ST4_06            (g_mfg_top_base + 0x0C0)            /* 0x13FBF0C0 */
#define MFG_ACTIVE_POWER_CON_ST5_06            (g_mfg_top_base + 0x098)            /* 0x13FBF098 */
#define MFG_ACTIVE_POWER_CON_ST6_06            (g_mfg_top_base + 0x1C0)            /* 0x13FBF1C0 */
#define MFG_ACTIVE_POWER_CON_00                (g_mfg_top_base + 0x400)            /* 0x13FBF400 */
#define MFG_ACTIVE_POWER_CON_06                (g_mfg_top_base + 0x418)            /* 0x13FBF418 */
#define MFG_ACTIVE_POWER_CON_12                (g_mfg_top_base + 0x430)            /* 0x13FBF430 */
#define MFG_ACTIVE_POWER_CON_18                (g_mfg_top_base + 0x448)            /* 0x13FBF448 */
#define MFG_ACTIVE_POWER_CON_24                (g_mfg_top_base + 0x460)            /* 0x13FBF460 */
#define MFG_ACTIVE_POWER_CON_30                (g_mfg_top_base + 0x478)            /* 0x13FBF478 */
#define MFG_ACTIVE_POWER_CON_36                (g_mfg_top_base + 0x490)            /* 0x13FBF490 */
#define MFG_ACTIVE_POWER_CON_42                (g_mfg_top_base + 0x4A8)            /* 0x13FBF4A8 */
#define MFG_ACTIVE_POWER_CON_48                (g_mfg_top_base + 0x4C0)            /* 0x13FBF4C0 */
#define MFG_ACTIVE_POWER_CON_54                (g_mfg_top_base + 0x4D8)            /* 0x13FBF4D8 */
#define MFG_ACTIVE_POWER_CON_60                (g_mfg_top_base + 0x4F0)            /* 0x13FBF4F0 */
#define MFG_ACTIVE_POWER_CON_66                (g_mfg_top_base + 0x548)            /* 0x13FBF548 */
#define MFG_RTU_ACTIVE_POWER_CON_00            (g_mfg_top_base + 0x700)            /* 0x13FBF700 */
#define MFG_RTU_ACTIVE_POWER_CON_06            (g_mfg_top_base + 0x718)            /* 0x13FBF718 */
#define MFG_RTU_ACTIVE_POWER_CON_12            (g_mfg_top_base + 0x730)            /* 0x13FBF730 */
#define MFG_RTU_ACTIVE_POWER_CON_18            (g_mfg_top_base + 0x748)            /* 0x13FBF748 */
#define MFG_RTU_ACTIVE_POWER_CON_24            (g_mfg_top_base + 0x760)            /* 0x13FBF760 */
#define MFG_RTU_ACTIVE_POWER_CON_30            (g_mfg_top_base + 0x778)            /* 0x13FBF778 */
#define MFG_RTU_ACTIVE_POWER_CON_36            (g_mfg_top_base + 0x790)            /* 0x13FBF790 */
#define MFG_RTU_ACTIVE_POWER_CON_42            (g_mfg_top_base + 0x7A8)            /* 0x13FBF7A8 */
#define MFG_RTU_ACTIVE_POWER_CON_48            (g_mfg_top_base + 0x7C0)            /* 0x13FBF7C0 */
#define MFG_RTU_ACTIVE_POWER_CON_54            (g_mfg_top_base + 0x7D8)            /* 0x13FBF7D8 */
#define MFG_RTU_ACTIVE_POWER_CON_60            (g_mfg_top_base + 0x7F0)            /* 0x13FBF7F0 */
#define MFG_RTU_ACTIVE_POWER_CON_66            (g_mfg_top_base + 0x808)            /* 0x13FBF808 */
#define MFG_I2M_PROTECTOR_CFG_00               (g_mfg_top_base + 0xF60)            /* 0x13FBFF60 */
#define MFG_I2M_PROTECTOR_CFG_01               (g_mfg_top_base + 0xF64)            /* 0x13FBFF64 */
#define MFG_I2M_PROTECTOR_CFG_02               (g_mfg_top_base + 0xF68)            /* 0x13FBFF68 */
#define MFG_AXCOHERENCE_CON                    (g_mfg_top_base + 0x168)            /* 0x13FBF168 */
#define MFG_DISPATCH_DRAM_ACP_TCM_CON_6        (g_mfg_top_base + 0x2B8)            /* 0x13FBF2B8 */
#define MFG_2TO1AXI_CON_0                      (g_mfg_top_base + 0x8F0)            /* 0x13FBF8F0 */
#define MFG_OUT_2TO1AXI_CON_0                  (g_mfg_top_base + 0x8F4)            /* 0x13FBF8F4 */
#define MFG_TCU_DRAM_2TO1AXI_CON               (g_mfg_top_base + 0x300)            /* 0x13FBF300 */
#define MFG_2TO1AXI_BYPASS_PIPE_1              (g_mfg_top_base + 0x308)            /* 0x13FBF308 */
#define MFG_2TO1AXI_BYPASS_PIPE_2              (g_mfg_top_base + 0x30C)            /* 0x13FBF30C */
#define MFG_2TO1AXI_BYPASS_PIPE_ENABLE         (g_mfg_top_base + 0x6FC)            /* 0x13FBF6FC */
#define MFG_2TO1AXI_PRIORITY                   (g_mfg_top_base + 0x5D0)            /* 0x13FBF5D0 */
#define MFG_MERGE_R_CON_00                     (g_mfg_top_base + 0x8A0)            /* 0x13FBF8A0 */
#define MFG_MERGE_R_CON_02                     (g_mfg_top_base + 0x8A8)            /* 0x13FBF8A8 */
#define MFG_MERGE_R_CON_04                     (g_mfg_top_base + 0x8C0)            /* 0x13FBF8C0 */
#define MFG_MERGE_R_CON_06                     (g_mfg_top_base + 0x8C8)            /* 0x13FBF8C8 */
#define MFG_MERGE_W_CON_00                     (g_mfg_top_base + 0x8B0)            /* 0x13FBF8B0 */
#define MFG_MERGE_W_CON_02                     (g_mfg_top_base + 0x8B8)            /* 0x13FBF8B8 */
#define MFG_MERGE_W_CON_04                     (g_mfg_top_base + 0x8D0)            /* 0x13FBF8D0 */
#define MFG_MERGE_W_CON_06                     (g_mfg_top_base + 0x8D8)            /* 0x13FBF8D8 */
#define MFG_MERGE_R_CON_08                     (g_mfg_top_base + 0x9C0)            /* 0x13FBF9C0 */
#define MFG_MERGE_R_CON_09                     (g_mfg_top_base + 0x9C4)            /* 0x13FBF9C4 */
#define MFG_MERGE_R_CON_10                     (g_mfg_top_base + 0x9C8)            /* 0x13FBF9C8 */
#define MFG_MERGE_R_CON_11                     (g_mfg_top_base + 0x9CC)            /* 0x13FBF9CC */
#define MFG_MERGE_W_CON_08                     (g_mfg_top_base + 0x9D0)            /* 0x13FBF9D0 */
#define MFG_MERGE_W_CON_09                     (g_mfg_top_base + 0x9D4)            /* 0x13FBF9D4 */
#define MFG_MERGE_W_CON_10                     (g_mfg_top_base + 0x9D8)            /* 0x13FBF9D8 */
#define MFG_MERGE_W_CON_11                     (g_mfg_top_base + 0x9DC)            /* 0x13FBF9DC */
#define MFG_DEBUGMON_CON_00                    (g_mfg_top_base + 0x8F8)            /* 0x13FBF8F8 */
#define MFG_DFD_CON_0                          (g_mfg_top_base + 0xA00)            /* 0x13FBFA00 */
#define MFG_DFD_CON_1                          (g_mfg_top_base + 0xA04)            /* 0x13FBFA04 */
#define MFG_DFD_CON_2                          (g_mfg_top_base + 0xA08)            /* 0x13FBFA08 */
#define MFG_DFD_CON_3                          (g_mfg_top_base + 0xA0C)            /* 0x13FBFA0C */
#define MFG_DFD_CON_4                          (g_mfg_top_base + 0xA10)            /* 0x13FBFA10 */
#define MFG_DFD_CON_5                          (g_mfg_top_base + 0xA14)            /* 0x13FBFA14 */
#define MFG_DFD_CON_6                          (g_mfg_top_base + 0xA18)            /* 0x13FBFA18 */
#define MFG_DFD_CON_7                          (g_mfg_top_base + 0xA1C)            /* 0x13FBFA1C */
#define MFG_DFD_CON_8                          (g_mfg_top_base + 0xA20)            /* 0x13FBFA20 */
#define MFG_DFD_CON_9                          (g_mfg_top_base + 0xA24)            /* 0x13FBFA24 */
#define MFG_DFD_CON_10                         (g_mfg_top_base + 0xA28)            /* 0x13FBFA28 */
#define MFG_DFD_CON_11                         (g_mfg_top_base + 0xA2C)            /* 0x13FBFA2C */
#define MFG_DFD_CON_20                         (g_mfg_top_base + 0xCD0)            /* 0x13FBFCD0 */
#define MFG_DFD_CON_21                         (g_mfg_top_base + 0xCD4)            /* 0x13FBFCD4 */
#define MFG_DFD_CON_22                         (g_mfg_top_base + 0xCD8)            /* 0x13FBFCD8 */
#define MFG_DFD_CON_23                         (g_mfg_top_base + 0xCDC)            /* 0x13FBFCDC */
#define MFG_DFD_CON_24                         (g_mfg_top_base + 0xBFC)            /* 0x13FBFBFC */
#define MFG_DFD_CON_25                         (g_mfg_top_base + 0xF24)            /* 0x13FBFF24 */
#define MFG_DFD_CON_26                         (g_mfg_top_base + 0xF28)            /* 0x13FBFF28 */
#define MFG_DFD_CON_27                         (g_mfg_top_base + 0xF2C)            /* 0x13FBFF2C */
#define MFG_PDCA_BACKDOOR                      (g_mfg_top_base + 0x210)            /* 0x13FBF210 */
#define MFG_DREQ_TOP_DBG_CON_0                 (g_mfg_top_base + 0x1F0)            /* 0x13FBF1F0 */
#define MFG_SRAM_FUL_SEL_ULV                   (g_mfg_top_base + 0x080)            /* 0x13FBF080 */
#define MFG_SRAM_FUL_SEL_ULV_TOP               (g_mfg_top_base + 0x084)            /* 0x13FBF084 */
#define MFG_POWER_TRACKER_SETTING              (g_mfg_top_base + 0xFE0)            /* 0x13FBFFE0 */
#define MFG_POWER_TRACKER_PDC_STATUS0          (g_mfg_top_base + 0xFE4)            /* 0x13FBFFE4 */
#define MFG_POWER_TRACKER_PDC_STATUS1          (g_mfg_top_base + 0xFE8)            /* 0x13FBFFE8 */
#define MFG_POWER_TRACKER_PDC_STATUS2          (g_mfg_top_base + 0xFEC)            /* 0x13FBFFEC */
#define MFG_POWER_TRACKER_PDC_STATUS3          (g_mfg_top_base + 0xFF0)            /* 0x13FBFFF0 */
#define MFG_POWER_TRACKER_PDC_STATUS4          (g_mfg_top_base + 0xFF4)            /* 0x13FBFFF4 */
#define MFG_POWER_TRACKER_PDC_STATUS5          (g_mfg_top_base + 0xFF8)            /* 0x13FBFFF8 */

#define MFG_CG_CFG_BASE                        (0x13E90000)
#define MFG_CG_DUMMY_REG                       (g_mfg_cg_base + 0x000)             /* 0x13E90000 */
#define MFG_CG_SRAM_FUL_SEL_ULV                (g_mfg_cg_base + 0x080)             /* 0x13E90080 */
#define MFG_CG_HW_DELSEL_SEL_0                 (g_mfg_cg_base + 0x084)             /* 0x13E90084 */
#define MFG_CG_HW_DELSEL_SEL_1                 (g_mfg_cg_base + 0x088)             /* 0x13E90088 */
#define MFG_CG_DREQ_CG_DBG_CON_0               (g_mfg_cg_base + 0x08C)             /* 0x13E9008C */

#define MFG_PLL_BASE                           (0x13FA0000)
#define MFG_PLL_CON0                           (g_mfg_pll_base + 0x008)            /* 0x13FA0008 */
#define MFG_PLL_CON1                           (g_mfg_pll_base + 0x00C)            /* 0x13FA000C */
#define MFG_PLL_FQMTR_CON0                     (g_mfg_pll_base + 0x040)            /* 0x13FA0040 */
#define MFG_PLL_FQMTR_CON1                     (g_mfg_pll_base + 0x044)            /* 0x13FA0044 */

#define MFG_PLL_SC0_BASE                       (0x13FA0400)
#define MFG_PLL_SC0_CON0                       (g_mfg_pll_sc0_base + 0x008)        /* 0x13FA0408 */
#define MFG_PLL_SC0_CON1                       (g_mfg_pll_sc0_base + 0x00C)        /* 0x13FA040C */
#define MFG_PLL_SC0_FQMTR_CON0                 (g_mfg_pll_sc0_base + 0x040)        /* 0x13FA0440 */
#define MFG_PLL_SC0_FQMTR_CON1                 (g_mfg_pll_sc0_base + 0x044)        /* 0x13FA0444 */

#define MFG_PLL_SC1_BASE                       (0x13FA0800)
#define MFG_PLL_SC1_CON0                       (g_mfg_pll_sc1_base + 0x008)        /* 0x13FA0808 */
#define MFG_PLL_SC1_CON1                       (g_mfg_pll_sc1_base + 0x00C)        /* 0x13FA080C */
#define MFG_PLL_SC1_FQMTR_CON0                 (g_mfg_pll_sc1_base + 0x040)        /* 0x13FA0840 */
#define MFG_PLL_SC1_FQMTR_CON1                 (g_mfg_pll_sc1_base + 0x044)        /* 0x13FA0844 */

#define MFG_RPC_BASE                           (0x13F90000)
#define MFG_RPC_GPUEB_TO_MCU_ACP_SW_PROT       (g_mfg_rpc_base + 0x0554)           /* 0x13F90554 */
#define MFG_RPC_MFG_CK_FAST_REF_SEL            (g_mfg_rpc_base + 0x055C)           /* 0x13F9055C */
#define MFG_RPC_DUMMY_REG                      (g_mfg_rpc_base + 0x0568)           /* 0x13F90568 */
#define MFG_RPC_MFG1_PWR_CON                   (g_mfg_rpc_base + 0x1070)           /* 0x13F91070 */
#define MFG_RPC_MFG2_PWR_CON                   (g_mfg_rpc_base + 0x10A0)           /* 0x13F910A0 */
#define MFG_RPC_MFG3_PWR_CON                   (g_mfg_rpc_base + 0x10A4)           /* 0x13F910A4 */
#define MFG_RPC_MFG4_PWR_CON                   (g_mfg_rpc_base + 0x10A8)           /* 0x13F910A8 */
#define MFG_RPC_MFG6_PWR_CON                   (g_mfg_rpc_base + 0x10B0)           /* 0x13F910B0 */
#define MFG_RPC_MFG7_PWR_CON                   (g_mfg_rpc_base + 0x10B4)           /* 0x13F910B4 */
#define MFG_RPC_MFG8_PWR_CON                   (g_mfg_rpc_base + 0x10B8)           /* 0x13F910B8 */
#define MFG_RPC_MFG9_PWR_CON                   (g_mfg_rpc_base + 0x10BC)           /* 0x13F910BC */
#define MFG_RPC_MFG10_PWR_CON                  (g_mfg_rpc_base + 0x10C0)           /* 0x13F910C0 */
#define MFG_RPC_MFG11_PWR_CON                  (g_mfg_rpc_base + 0x10C4)           /* 0x13F910C4 */
#define MFG_RPC_MFG12_PWR_CON                  (g_mfg_rpc_base + 0x10C8)           /* 0x13F910C8 */
#define MFG_RPC_MFG13_PWR_CON                  (g_mfg_rpc_base + 0x10CC)           /* 0x13F910CC */
#define MFG_RPC_MFG14_PWR_CON                  (g_mfg_rpc_base + 0x10D0)           /* 0x13F910D0 */
#define MFG_RPC_MFG15_PWR_CON                  (g_mfg_rpc_base + 0x10D4)           /* 0x13F910D4 */
#define MFG_RPC_MFG16_PWR_CON                  (g_mfg_rpc_base + 0x10D8)           /* 0x13F910D8 */
#define MFG_RPC_MFG17_PWR_CON                  (g_mfg_rpc_base + 0x10DC)           /* 0x13F910DC */
#define MFG_RPC_MFG18_PWR_CON                  (g_mfg_rpc_base + 0x10E0)           /* 0x13F910E0 */
#define MFG_RPC_MFG19_PWR_CON                  (g_mfg_rpc_base + 0x10E4)           /* 0x13F910E4 */
#define MFG_RPC_MFG20_PWR_CON                  (g_mfg_rpc_base + 0x10E8)           /* 0x13F910E8 */
#define MFG_RPC_MFG22_PWR_CON                  (g_mfg_rpc_base + 0x10F0)           /* 0x13F910F0 */
#define MFG_RPC_MFG25_PWR_CON                  (g_mfg_rpc_base + 0x10FC)           /* 0x13F910FC */
#define MFG_RPC_MFG26_PWR_CON                  (g_mfg_rpc_base + 0x1600)           /* 0x13F91600 */
#define MFG_RPC_MFG27_PWR_CON                  (g_mfg_rpc_base + 0x1604)           /* 0x13F91604 */
#define MFG_RPC_MFG28_PWR_CON                  (g_mfg_rpc_base + 0x1608)           /* 0x13F91608 */
#define MFG_RPC_MFG29_PWR_CON                  (g_mfg_rpc_base + 0x160C)           /* 0x13F9160C */
#define MFG_RPC_MFG30_PWR_CON                  (g_mfg_rpc_base + 0x1610)           /* 0x13F91610 */
#define MFG_RPC_MFG31_PWR_CON                  (g_mfg_rpc_base + 0x1614)           /* 0x13F91614 */
#define MFG_RPC_MFG32_PWR_CON                  (g_mfg_rpc_base + 0x1618)           /* 0x13F91618 */
#define MFG_RPC_MFG33_PWR_CON                  (g_mfg_rpc_base + 0x161C)           /* 0x13F9161C */
#define MFG_RPC_MFG34_PWR_CON                  (g_mfg_rpc_base + 0x1620)           /* 0x13F91620 */
#define MFG_RPC_MFG35_PWR_CON                  (g_mfg_rpc_base + 0x1624)           /* 0x13F91624 */
#define MFG_RPC_MFG36_PWR_CON                  (g_mfg_rpc_base + 0x1628)           /* 0x13F91628 */
#define MFG_RPC_MFG37_PWR_CON                  (g_mfg_rpc_base + 0x162C)           /* 0x13F9162C */
#define MFG_RPC_MFG_PWR_CON_STATUS             (g_mfg_rpc_base + 0x1200)           /* 0x13F91200 */
#define MFG_RPC_MFG_PWR_CON_2ND_STATUS         (g_mfg_rpc_base + 0x1204)           /* 0x13F91204 */
#define MFG_RPC_MFG_PWR_CON_STATUS_1           (g_mfg_rpc_base + 0x1208)           /* 0x13F91208 */
#define MFG_RPC_MFG_PWR_CON_2ND_STATUS_1       (g_mfg_rpc_base + 0x120C)           /* 0x13F9120C */
#define MFG_RPC_SLP_PROT_EN_SET                (g_mfg_rpc_base + 0x1040)           /* 0x13F91040 */
#define MFG_RPC_SLP_PROT_EN_CLR                (g_mfg_rpc_base + 0x1044)           /* 0x13F91044 */
#define MFG_RPC_SLP_PROT_EN_STA                (g_mfg_rpc_base + 0x1048)           /* 0x13F91048 */
#define MFG_RPC_GPUEB_CFG                      (g_mfg_rpc_base + 0x1030)           /* 0x13F91030 */
#define MFG_RPC_CK_FAST_REF_SEL                (g_mfg_rpc_base + 0x055C)           /* 0x13F9055C */
#define MFG_RPC_GTOP_DREQ_CFG                  (g_mfg_rpc_base + 0x1110)           /* 0x13F91110 */
#define MFG_RPC_AO_CLK_CFG                     (g_mfg_rpc_base + 0x1034)           /* 0x13F91034 */
#define MFG_RPC_BRISKET_TOP_AO_CFG_0           (g_mfg_rpc_base + 0x1100)           /* 0x13F91100 */
#define MFG_RPC_BRISKET_ST0_AO_CFG_0           (g_mfg_rpc_base + 0x0500)           /* 0x13F90500 */
#define MFG_RPC_BRISKET_ST1_AO_CFG_0           (g_mfg_rpc_base + 0x0504)           /* 0x13F90504 */
#define MFG_RPC_BRISKET_ST3_AO_CFG_0           (g_mfg_rpc_base + 0x050C)           /* 0x13F9050C */
#define MFG_RPC_BRISKET_ST4_AO_CFG_0           (g_mfg_rpc_base + 0x0510)           /* 0x13F90510 */
#define MFG_RPC_BRISKET_ST5_AO_CFG_0           (g_mfg_rpc_base + 0x0514)           /* 0x13F90514 */
#define MFG_RPC_BRISKET_ST6_AO_CFG_0           (g_mfg_rpc_base + 0x0518)           /* 0x13F90518 */

#define MFG_AXUSER_BASE                        (0x13FC0000)
#define MFG_AXUSER_CFG_01                      (g_mfg_axuser_base + 0x704)         /* 0x13FC0704 */
#define MFG_AXUSER_CFG_02                      (g_mfg_axuser_base + 0x708)         /* 0x13FC0704 */
#define MFG_AXUSER_CFG_03                      (g_mfg_axuser_base + 0x70C)         /* 0x13FC0704 */
#define MFG_AXUSER_R_BYPASS_MRG_BY_HINT_CFG    (g_mfg_axuser_base + 0x820)         /* 0x13FC0820 */

#define MFG_BRCAST_BASE                        (0x13FB1000)
#define MFG_BRCAST_TEST_MODE_2                 (g_mfg_brcast_base + 0xF20)         /* 0x13FB1F20 */
#define MFG_BRCAST_CONFIG_1                    (g_mfg_brcast_base + 0xFEC)         /* 0x13FB1FEC */
#define MFG_BRCAST_CONFIG_4                    (g_mfg_brcast_base + 0xFF8)         /* 0x13FB1FF8 */
#define MFG_BRCAST_CONFIG_5                    (g_mfg_brcast_base + 0xFFC)         /* 0x13FB1FFC */
#define MFG_BRCAST_START_ADDR_3                (g_mfg_brcast_base + 0x418)         /* 0x13FB1418 */
#define MFG_BRCAST_END_ADDR_3                  (g_mfg_brcast_base + 0x41C)         /* 0x13FB141C */
#define MFG_BRCAST_START_ADDR_4                (g_mfg_brcast_base + 0x420)         /* 0x13FB1420 */
#define MFG_BRCAST_END_ADDR_4                  (g_mfg_brcast_base + 0x424)         /* 0x13FB1424 */
#define MFG_BRCAST_START_ADDR_5                (g_mfg_brcast_base + 0x428)         /* 0x13FB1428 */
#define MFG_BRCAST_END_ADDR_5                  (g_mfg_brcast_base + 0x42C)         /* 0x13FB142C */
#define MFG_BRCAST_START_ADDR_6                (g_mfg_brcast_base + 0x430)         /* 0x13FB1430 */
#define MFG_BRCAST_END_ADDR_6                  (g_mfg_brcast_base + 0x434)         /* 0x13FB1434 */
#define MFG_BRCAST_START_ADDR_7                (g_mfg_brcast_base + 0x438)         /* 0x13FB1438 */
#define MFG_BRCAST_END_ADDR_7                  (g_mfg_brcast_base + 0x43C)         /* 0x13FB143C */
#define MFG_BRCAST_START_ADDR_8                (g_mfg_brcast_base + 0x440)         /* 0x13FB1440 */
#define MFG_BRCAST_END_ADDR_8                  (g_mfg_brcast_base + 0x444)         /* 0x13FB1444 */
#define MFG_BRCAST_START_ADDR_42               (g_mfg_brcast_base + 0x550)         /* 0x13FB1550 */
#define MFG_BRCAST_END_ADDR_42                 (g_mfg_brcast_base + 0x554)         /* 0x13FB1554 */
#define MFG_BRCAST_START_END_ADDR_PTY_0_16     (g_mfg_brcast_base + 0x6A0)         /* 0x13FB16A0 */
#define MFG_BRCAST_PROG_DATA_114               (g_mfg_brcast_base + 0x9C8)         /* 0x13FB19C8 */
#define MFG_BRCAST_PROG_DATA_141               (g_mfg_brcast_base + 0xA34)         /* 0x13FB1A34 */
#define MFG_BRCAST_PROG_DATA_142               (g_mfg_brcast_base + 0xA38)         /* 0x13FB1A38 */
#define MFG_BRCAST_CMD_SEQ_0_0_LSB             (g_mfg_brcast_base + 0x000)         /* 0x13FB1000 */
#define MFG_BRCAST_CMD_SEQ_0_0_MSB             (g_mfg_brcast_base + 0x004)         /* 0x13FB1004 */
#define MFG_BRCAST_CMD_SEQ_0_1_LSB             (g_mfg_brcast_base + 0x008)         /* 0x13FB1008 */
#define MFG_BRCAST_CMD_SEQ_0_1_MSB             (g_mfg_brcast_base + 0x00C)         /* 0x13FB100C */
#define MFG_BRCAST_CMD_SEQ_0_11_LSB            (g_mfg_brcast_base + 0x058)         /* 0x13FB1058 */
#define MFG_BRCAST_CMD_SEQ_0_11_MSB            (g_mfg_brcast_base + 0x05C)         /* 0x13FB105C */
#define MFG_BRCAST_CMD_SEQ_0_12_LSB            (g_mfg_brcast_base + 0x060)         /* 0x13FB1060 */
#define MFG_BRCAST_CMD_SEQ_0_12_MSB            (g_mfg_brcast_base + 0x064)         /* 0x13FB1064 */
#define MFG_BRCAST_CMD_SEQ_1_0_LSB             (g_mfg_brcast_base + 0x080)         /* 0x13FB1080 */
#define MFG_BRCAST_CMD_SEQ_1_0_MSB             (g_mfg_brcast_base + 0x084)         /* 0x13FB1084 */
#define MFG_BRCAST_CMD_SEQ_0AND1_PTY           (g_mfg_brcast_base + 0x380)         /* 0x13FB1380 */
#define MFG_BRCAST_ERROR_IRQ                   (g_mfg_brcast_base + 0xDFC)         /* 0x13FB1DFC */
#define MFG_BRCAST_DEBUG_INFO_9                (g_mfg_brcast_base + 0xE2C)         /* 0x13FB1E2C */
#define MFG_BRCAST_DEBUG_INFO_10               (g_mfg_brcast_base + 0xE30)         /* 0x13FB1E30 */
#define MFG_BRCAST_DEBUG_INFO_11               (g_mfg_brcast_base + 0xE34)         /* 0x13FB1E34 */
#define MFG_BRCAST_DEBUG_INFO_12               (g_mfg_brcast_base + 0xE38)         /* 0x13FB1E38 */
#define MFG_BRCAST_DEBUG_INFO_13               (g_mfg_brcast_base + 0xE3C)         /* 0x13FB1E3C */
#define MFG_BRCAST_CONFIG_0                    (g_mfg_brcast_base + 0xFE8)         /* 0x13FB1FE8 */
#define MFG_BRCAST_CONFIG_1                    (g_mfg_brcast_base + 0xFEC)         /* 0x13FB1FEC */
#define MFG_BRCAST_CONFIG_2                    (g_mfg_brcast_base + 0xFF0)         /* 0x13FB1FF0 */
#define MFG_BRCAST_CONFIG_3                    (g_mfg_brcast_base + 0xFF4)         /* 0x13FB1FF4 */
#define MFG_BRCAST_CONFIG_4                    (g_mfg_brcast_base + 0xFF8)         /* 0x13FB1FF8 */
#define MFG_BRCAST_CONFIG_5                    (g_mfg_brcast_base + 0xFFC)         /* 0x13FB1FFC */

#define MFG_VGPU_DEVAPC_AO_BASE                (0x13FA1000)
#define MFG_VGPU_DEVAPC_AO_MAS_SEC_0           (g_mfg_vgpu_devapc_ao_base + 0xA00) /* 0x13FA1A00 */
#define MFG_VGPU_DEVAPC_AO_APC_CON             (g_mfg_vgpu_devapc_ao_base + 0xF00) /* 0x13FA1F00 */

#define MFG_VGPU_DEVAPC_BASE                   (0x13FA2000)
#define MFG_VGPU_DEVAPC_D0_VIO_STA_0           (g_mfg_vgpu_devapc_base + 0x400)    /* 0x13FA2400 */
#define MFG_VGPU_DEVAPC_D0_VIO_STA_1           (g_mfg_vgpu_devapc_base + 0x404)    /* 0x13FA2404 */

#define MFG_SMMU_BASE                          (0x13A00000)
#define MFG_SMMU_CR0                           (g_mfg_smmu_base + 0x0020)          /* 0x13A00000 */
#define MFG_SMMU_GBPA                          (g_mfg_smmu_base + 0x0044)          /* 0x13A00000 */

#define MFG_HBVC_BASE                          (0x13F50000)
#define MFG_HBVC_CFG                           (g_mfg_hbvc_base + 0x000)           /* 0x13F50000 */
#define MFG_HBVC_SAMPLE_EN                     (g_mfg_hbvc_base + 0x004)           /* 0x13F50004 */
#define MFG_HBVC_GRP0_CFG                      (g_mfg_hbvc_base + 0x0C0)           /* 0x13F500C0 */
#define MFG_HBVC_GRP1_CFG                      (g_mfg_hbvc_base + 0x0C4)           /* 0x13F500C4 */
#define MFG_HBVC_GRP0_VPROC_UPDATE             (g_mfg_hbvc_base + 0x1C0)           /* 0x13F501C0 */
#define MFG_HBVC_GRP1_VPROC_UPDATE             (g_mfg_hbvc_base + 0x1C4)           /* 0x13F501C4 */
#define MFG_HBVC_FLL0_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x400)           /* 0x13F50400 */
#define MFG_HBVC_FLL1_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x404)           /* 0x13F50404 */
#define MFG_HBVC_FLL2_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x408)           /* 0x13F50408 */
#define MFG_HBVC_FLL4_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x410)           /* 0x13F50410 */
#define MFG_HBVC_FLL5_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x414)           /* 0x13F50414 */
#define MFG_HBVC_FLL6_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x418)           /* 0x13F50418 */
#define MFG_HBVC_FLL7_DBG_FRONTEND0            (g_mfg_hbvc_base + 0x41C)           /* 0x13F5041C */
#define MFG_HBVC_GRP0_DBG_BACKEND0             (g_mfg_hbvc_base + 0x480)           /* 0x13F50480 */
#define MFG_HBVC_GRP1_DBG_BACKEND0             (g_mfg_hbvc_base + 0x484)           /* 0x13F50484 */
#define HBVC_DBG_PMIFARB0                      (g_mfg_hbvc_base + 0x4C0)           /* 0x13F504C0 */
#define HBVC_DBG_PMIFARB1                      (g_mfg_hbvc_base + 0x4C4)           /* 0x13F504C4 */
#define HBVC_DBG_PMIFARB2                      (g_mfg_hbvc_base + 0x4C8)           /* 0x13F504C8 */
#define HBVC_PMIF_ERR_SEL                      (g_mfg_hbvc_base + 0x4D0)           /* 0x13F504D0 */
#define HBVC_PMIF_ERR_OUT_L                    (g_mfg_hbvc_base + 0x4D4)           /* 0x13F504D4 */
#define HBVC_PMIF_ERR_OUT_H                    (g_mfg_hbvc_base + 0x4D8)           /* 0x13F504D8 */
#define HBVC_STATUS                            (g_mfg_hbvc_base + 0x4DC)           /* 0x13F504DC */
#define MFG_HBVC_GRP1_POC_CFG                  (g_mfg_hbvc_base + 0x810)           /* 0x13F50810 */
#define MFG_HBVC_GRP1_POC_DBG_CFG              (g_mfg_hbvc_base + 0x814)           /* 0x13F50814 */
#define MFG_HBVC_GRP1_POC_DBG_0                (g_mfg_hbvc_base + 0x818)           /* 0x13F50818 */
#define MFG_HBVC_GRP1_POC_DBG_1                (g_mfg_hbvc_base + 0x81C)           /* 0x13F5081C */

#define BRISKET_TOP_BASE                       (0x13FB0000)
#define BRISKET_TOP_VOLTAGEEXT                 (g_brisket_top_base + 0x148)        /* 0x13FB0148 */

#define BRISKET_ST0_BASE                       (0x13E1C000)
#define BRISKET_ST0_VOLTAGEEXT                 (g_brisket_st0_base + 0x148)        /* 0x13E1C148 */

#define BRISKET_ST1_BASE                       (0x13E2C000)
#define BRISKET_ST1_VOLTAGEEXT                 (g_brisket_st1_base + 0x148)        /* 0x13E2C148 */

#define BRISKET_ST3_BASE                       (0x13E4C000)
#define BRISKET_ST3_VOLTAGEEXT                 (g_brisket_st3_base + 0x148)        /* 0x13E4C148 */

#define BRISKET_ST4_BASE                       (0x13E5C000)
#define BRISKET_ST4_VOLTAGEEXT                 (g_brisket_st4_base + 0x148)        /* 0x13E5C148 */

#define BRISKET_ST5_BASE                       (0x13E6C000)
#define BRISKET_ST5_VOLTAGEEXT                 (g_brisket_st5_base + 0x148)        /* 0x13E6C148 */

#define BRISKET_ST6_BASE                       (0x13E7C000)
#define BRISKET_ST6_VOLTAGEEXT                 (g_brisket_st6_base + 0x148)        /* 0x13E7C148 */

#define SPM_BASE                               (0x1C001000)
#define SPM_SPM2GPUPM_CON                      (g_sleep + 0x410)                   /* 0x1C001410 */
#define SPM_MFG0_PWR_CON                       (g_sleep + 0xF04)                   /* 0x1C001F04 */
#define SPM_XPU_PWR_STATUS                     (g_sleep + 0xF80)                   /* 0x1C001F80 */
#define SPM_XPU_PWR_STATUS_2ND                 (g_sleep + 0xF84)                   /* 0x1C001F84 */
#define SPM_SOC_BUCK_ISO_CON                   (g_sleep + 0xF64)                   /* 0x1C001F64 */
#define SPM_SOC_BUCK_ISO_CON_SET               (g_sleep + 0xF68)                   /* 0x1C001F68 */
#define SPM_SOC_BUCK_ISO_CON_CLR               (g_sleep + 0xF6C)                   /* 0x1C001F6C */
#define SPM_SRC_REQ                            (g_sleep + 0x818)                   /* 0x1C001818 */

#define TOPCKGEN_BASE                          (0x10000000)
#define TOPCK_CLK_CFG_5_STA                    (g_topckgen_base + 0x060)           /* 0x10000060 */

#define IFRBUS_AO_BASE                         (0x1002C000)
#define IFR_EMISYS_PROTECT_EN_STA_0            (g_ifrbus_ao_base + 0x100)          /* 0x1002C100 */
#define IFR_EMISYS_PROTECT_EN_W1S_0            (g_ifrbus_ao_base + 0x104)          /* 0x1002C104 */
#define IFR_EMISYS_PROTECT_EN_W1C_0            (g_ifrbus_ao_base + 0x108)          /* 0x1002C108 */
#define IFR_EMISYS_PROTECT_RDY_STA_0           (g_ifrbus_ao_base + 0x10C)          /* 0x1002C10C */
#define IFR_EMISYS_PROTECT_EN_STA_1            (g_ifrbus_ao_base + 0x120)          /* 0x1002C120 */
#define IFR_EMISYS_PROTECT_EN_W1S_1            (g_ifrbus_ao_base + 0x124)          /* 0x1002C124 */
#define IFR_EMISYS_PROTECT_EN_W1C_1            (g_ifrbus_ao_base + 0x128)          /* 0x1002C128 */
#define IFR_EMISYS_PROTECT_RDY_STA_1           (g_ifrbus_ao_base + 0x12C)          /* 0x1002C12C */
#define IFR_INFRASYS_PROTECT_EN_STA_0          (g_ifrbus_ao_base + 0x000)          /* 0x1002C000 */
#define IFR_INFRASYS_PROTECT_EN_W1S_0          (g_ifrbus_ao_base + 0x004)          /* 0x1002C004 */
#define IFR_INFRASYS_PROTECT_EN_W1C_0          (g_ifrbus_ao_base + 0x008)          /* 0x1002C008 */
#define IFR_INFRASYS_PROTECT_RDY_STA_0         (g_ifrbus_ao_base + 0x00C)          /* 0x1002C00C */
#define IFR_MFGSYS_PROTECT_EN_STA_0            (g_ifrbus_ao_base + 0x1A0)          /* 0x1002C1A0 */
#define IFR_MFGSYS_PROTECT_EN_W1S_0            (g_ifrbus_ao_base + 0x1A4)          /* 0x1002C1A4 */
#define IFR_MFGSYS_PROTECT_EN_W1C_0            (g_ifrbus_ao_base + 0x1A8)          /* 0x1002C1A8 */
#define IFR_MFGSYS_PROTECT_RDY_STA_0           (g_ifrbus_ao_base + 0x1AC)          /* 0x1002C1AC */

#define EMI_BASE                               (0x10219000)
#define EMI_URGENT_CNT                         (g_emi_base + 0x81C)                /* 0x1021981C */
#define EMI_MD_LAT_HRT_URGENT_CNT              (g_emi_base + 0x860)                /* 0x10219860 */
#define EMI_MD_HRT_URGENT_CNT                  (g_emi_base + 0x864)                /* 0x10219864 */
#define EMI_DISP_HRT_URGENT_CNT                (g_emi_base + 0x868)                /* 0x10219868 */
#define EMI_CAM_HRT_URGENT_CNT                 (g_emi_base + 0x86C)                /* 0x1021986C */
#define EMI_MDMCU_HRT_URGENT_CNT               (g_emi_base + 0x870)                /* 0x10219870 */
#define EMI_MD_WR_LAT_HRT_URGENT_CNT           (g_emi_base + 0x9A4)                /* 0x102199A4 */
#define EMI_MDMCU_LOW_LAT_URGENT_CNT           (g_emi_base + 0xCC0)                /* 0x10219CC0 */
#define EMI_MDMCU_HIGH_LAT_URGENT_CNT          (g_emi_base + 0xCC4)                /* 0x10219CC4 */
#define EMI_MDMCU_LOW_WR_LAT_URGENT_CNT        (g_emi_base + 0xCC8)                /* 0x10219CC8 */
#define EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT       (g_emi_base + 0xCCC)                /* 0x10219CCC */

#define SUB_EMI_BASE                           (0x1021D000)
#define SUB_EMI_URGENT_CNT                     (g_sub_emi_base + 0x81C)            /* 0x1021D81C */
#define SUB_EMI_MD_LAT_HRT_URGENT_CNT          (g_sub_emi_base + 0x860)            /* 0x1021D860 */
#define SUB_EMI_MD_HRT_URGENT_CNT              (g_sub_emi_base + 0x864)            /* 0x1021D864 */
#define SUB_EMI_DISP_HRT_URGENT_CNT            (g_sub_emi_base + 0x868)            /* 0x1021D868 */
#define SUB_EMI_CAM_HRT_URGENT_CNT             (g_sub_emi_base + 0x86C)            /* 0x1021D86C */
#define SUB_EMI_MDMCU_HRT_URGENT_CNT           (g_sub_emi_base + 0x870)            /* 0x1021D870 */
#define SUB_EMI_MD_WR_LAT_HRT_URGENT_CNT       (g_sub_emi_base + 0x9A4)            /* 0x1021D9A4 */
#define SUB_EMI_MDMCU_LOW_LAT_URGENT_CNT       (g_sub_emi_base + 0xCC0)            /* 0x1021DCC0 */
#define SUB_EMI_MDMCU_HIGH_LAT_URGENT_CNT      (g_sub_emi_base + 0xCC4)            /* 0x1021DCC4 */
#define SUB_EMI_MDMCU_LOW_WR_LAT_URGENT_CNT    (g_sub_emi_base + 0xCC8)            /* 0x1021DCC8 */
#define SUB_EMI_MDMCU_HIGH_WR_LAT_URGENT_CNT   (g_sub_emi_base + 0xCCC)            /* 0x1021DCCC */

#define NTH_EMICFG_BASE                        (0x1021C000)
#define NTH_MFG_ACP_GALS_SLV_DBG               (g_nth_emicfg_base + 0x814)         /* 0x1021C814 */
#define NTH_APU_ACP_GALS_SLV_DBG               (g_nth_emicfg_base + 0x81C)         /* 0x1021C81C */
#define NTH_APU_EMI1_GALS_SLV_DBG              (g_nth_emicfg_base + 0x840)         /* 0x1021C840 */
#define NTH_APU_EMI0_GALS_SLV_DBG              (g_nth_emicfg_base + 0x844)         /* 0x1021C844 */
#define NTH_MFG_EMI1_GALS_SLV_DBG              (g_nth_emicfg_base + 0x848)         /* 0x1021C848 */
#define NTH_MFG_EMI0_GALS_SLV_DBG              (g_nth_emicfg_base + 0x84C)         /* 0x1021C84C */
#define NTH_MFG_ACP_DVM_GALS_MST_DBG           (g_nth_emicfg_base + 0x804)         /* 0x1021C804 */
#define NTH_MFG_ACP_GALS_SLV_DBG               (g_nth_emicfg_base + 0x814)         /* 0x1021C814 */

#define STH_EMICFG_BASE                        (0x1021E000)
#define STH_MFG_ACP_GALS_SLV_DBG               (g_sth_emicfg_base + 0x814)         /* 0x1021E814 */
#define STH_APU_ACP_GALS_SLV_DBG               (g_sth_emicfg_base + 0x81C)         /* 0x1021E81C */
#define STH_APU_EMI1_GALS_SLV_DBG              (g_sth_emicfg_base + 0x840)         /* 0x1021E840 */
#define STH_APU_EMI0_GALS_SLV_DBG              (g_sth_emicfg_base + 0x844)         /* 0x1021E844 */
#define STH_MFG_EMI1_GALS_SLV_DBG              (g_sth_emicfg_base + 0x848)         /* 0x1021E848 */
#define STH_MFG_EMI0_GALS_SLV_DBG              (g_sth_emicfg_base + 0x84C)         /* 0x1021E84C */
#define STH_MFG_ACP_DVM_GALS_MST_DBG           (g_sth_emicfg_base + 0x804)         /* 0x1021E804 */
#define STH_MFG_ACP_GALS_SLV_DBG               (g_sth_emicfg_base + 0x814)         /* 0x1021E814 */

#define NTH_EMICFG_AO_MEM_BASE                 (0x10270000)
#define NTH_AO_SLEEP_PROT_START                (g_nth_emicfg_ao_mem_base + 0x000)  /* 0x10270000 */
#define NTH_AO_GLITCH_PROT_START               (g_nth_emicfg_ao_mem_base + 0x080)  /* 0x10270080 */
#define NTH_AO_M6M7_IDLE_BIT_EN_1              (g_nth_emicfg_ao_mem_base + 0x228)  /* 0x10270228 */
#define NTH_AO_M6M7_IDLE_BIT_EN_0              (g_nth_emicfg_ao_mem_base + 0x22C)  /* 0x1027022C */

#define STH_EMICFG_AO_MEM_BASE                 (0x1030E000)
#define STH_AO_SLEEP_PROT_START                (g_sth_emicfg_ao_mem_base + 0x000)  /* 0x1030E000 */
#define STH_AO_GLITCH_PROT_START               (g_sth_emicfg_ao_mem_base + 0x080)  /* 0x1030E080 */
#define STH_AO_M6M7_IDLE_BIT_EN_1              (g_sth_emicfg_ao_mem_base + 0x228)  /* 0x1030E228 */
#define STH_AO_M6M7_IDLE_BIT_EN_0              (g_sth_emicfg_ao_mem_base + 0x22C)  /* 0x1030E22C */

#define INFRA_AO_DEBUG_CTRL_BASE               (0x10023000)
#define INFRA_AO_BUS0_U_DEBUG_CTRL0            (g_infra_ao_debug_ctrl + 0x000)     /* 0x10023000 */

#define INFRA_AO1_DEBUG_CTRL_BASE              (0x1002B000)
#define INFRA_AO1_BUS1_U_DEBUG_CTRL0           (g_infra_ao1_debug_ctrl + 0x000)    /* 0x1002B000 */

#define NTH_EMI_AO_DEBUG_CTRL_BASE             (0x10042000)
#define NTH_EMI_AO_BUS_U_DEBUG_CTRL0           (g_nth_emi_ao_debug_ctrl + 0x000)   /* 0x10042000 */

#define STH_EMI_AO_DEBUG_CTRL_BASE             (0x10028000)
#define STH_EMI_AO_BUS_U_DEBUG_CTRL0           (g_sth_emi_ao_debug_ctrl + 0x000)   /* 0x10028000 */

#define EFUSE_BASE                             (0x11F10000)
#define EFUSE_PTPOD27                          (g_efuse_base + 0xAA4)              /* 0x11F10AA4 */
#define EFUSE_PTPOD28                          (g_efuse_base + 0xAA8)              /* 0x11F10AA8 */
#define EFUSE_PTPOD29                          (g_efuse_base + 0xAAC)              /* 0x11F10AAC */
#define EFUSE_PTPOD30                          (g_efuse_base + 0xAB0)              /* 0x11F10AB0 */
#define EFUSE_PTPOD31                          (g_efuse_base + 0xAB4)              /* 0x11F10AB4 */
#define EFUSE_PTPOD32                          (g_efuse_base + 0xAB8)              /* 0x11F10AB8 */
#define EFUSE_PTPOD33                          (g_efuse_base + 0xABC)              /* 0x11F10ABC */

#define MFG_SECURE_BASE                        (0x13FBC000)
#define MFG_SECURE_SECU_ACP_FLT_CON            (g_mfg_secure_base + 0x000)         /* 0x13FBC000 */
#define MFG_SECURE_NORM_ACP_FLT_CON            (g_mfg_secure_base + 0x004)         /* 0x13FBC004 */
#define MFG_SECURE_REG                         (g_mfg_secure_base + 0xFE0)         /* 0x13FBCFE0 */

#define DRM_DEBUG_BASE                         (0x1000D000)
#define DRM_DEBUG_MFG_REG                      (g_drm_debug_base + 0x060)          /* 0x1000D060 */

#define NEMI_MI32_MI33_SMI_BASE                (0x1025E000)
#define NEMI_MI32_SMI_DEBUG_S0                 (g_nemi_mi32_mi33_smi + 0x400)      /* 0x1025E400 */
#define NEMI_MI32_SMI_DEBUG_S1                 (g_nemi_mi32_mi33_smi + 0x404)      /* 0x1025E404 */
#define NEMI_MI32_SMI_DEBUG_S2                 (g_nemi_mi32_mi33_smi + 0x408)      /* 0x1025E408 */
#define NEMI_MI32_SMI_DEBUG_M0                 (g_nemi_mi32_mi33_smi + 0x430)      /* 0x1025E430 */
#define NEMI_MI32_SMI_DEBUG_MISC               (g_nemi_mi32_mi33_smi + 0x440)      /* 0x1025E440 */
#define NEMI_MI33_SMI_DEBUG_S0                 (g_nemi_mi32_mi33_smi + 0xC00)      /* 0x1025EC00 */
#define NEMI_MI33_SMI_DEBUG_S1                 (g_nemi_mi32_mi33_smi + 0xC04)      /* 0x1025EC04 */
#define NEMI_MI33_SMI_DEBUG_S2                 (g_nemi_mi32_mi33_smi + 0xC08)      /* 0x1025EC08 */
#define NEMI_MI33_SMI_DEBUG_M0                 (g_nemi_mi32_mi33_smi + 0xC30)      /* 0x1025EC30 */
#define NEMI_MI33_SMI_DEBUG_MISC               (g_nemi_mi32_mi33_smi + 0xC40)      /* 0x1025EC40 */

#define SEMI_MI32_MI33_SMI_BASE                (0x10309000)
#define SEMI_MI32_SMI_DEBUG_S0                 (g_semi_mi32_mi33_smi + 0x400)      /* 0x10309400 */
#define SEMI_MI32_SMI_DEBUG_S1                 (g_semi_mi32_mi33_smi + 0x404)      /* 0x10309404 */
#define SEMI_MI32_SMI_DEBUG_S2                 (g_semi_mi32_mi33_smi + 0x408)      /* 0x10309408 */
#define SEMI_MI32_SMI_DEBUG_M0                 (g_semi_mi32_mi33_smi + 0x430)      /* 0x10309430 */
#define SEMI_MI32_SMI_DEBUG_MISC               (g_semi_mi32_mi33_smi + 0x440)      /* 0x10309440 */
#define SEMI_MI33_SMI_DEBUG_S0                 (g_semi_mi32_mi33_smi + 0xC00)      /* 0x10309C00 */
#define SEMI_MI33_SMI_DEBUG_S1                 (g_semi_mi32_mi33_smi + 0xC04)      /* 0x10309C04 */
#define SEMI_MI33_SMI_DEBUG_S2                 (g_semi_mi32_mi33_smi + 0xC08)      /* 0x10309C08 */
#define SEMI_MI33_SMI_DEBUG_M0                 (g_semi_mi32_mi33_smi + 0xC30)      /* 0x10309C30 */
#define SEMI_MI33_SMI_DEBUG_MISC               (g_semi_mi32_mi33_smi + 0xC40)      /* 0x10309C40 */

#define GPUEB_SRAM_BASE                        (0x13C00000)
#define GPUEB_SRAM_GPUEB_INIT_FOOTPRINT        (g_gpueb_sram_base + 0x4FD24)       /* 0x13C4FD24 */
#define GPUEB_SRAM_GPUFREQ_FOOTPRINT_GPR       (g_gpueb_sram_base + 0x4FD60)       /* 0x13C4FD60 */

#define GPUEB_CFGREG_BASE                      (0x13C60000)
#define GPUEB_AUTO_DMA_0_STATUS                (g_gpueb_cfgreg_base + 0x060)       /* 0x13C60060 */
#define GPUEB_AUTO_DMA_0_CUR_PC                (g_gpueb_cfgreg_base + 0x064)       /* 0x13C60064 */
#define GPUEB_AUTO_DMA_0_TRIGGER_STATUS        (g_gpueb_cfgreg_base + 0x06C)       /* 0x13C6006C */
#define GPUEB_AUTO_DMA_0_R0                    (g_gpueb_cfgreg_base + 0x070)       /* 0x13C60070 */
#define GPUEB_AUTO_DMA_0_R1                    (g_gpueb_cfgreg_base + 0x074)       /* 0x13C60074 */
#define GPUEB_AUTO_DMA_0_R2                    (g_gpueb_cfgreg_base + 0x078)       /* 0x13C60078 */
#define GPUEB_AUTO_DMA_0_R3                    (g_gpueb_cfgreg_base + 0x07C)       /* 0x13C6007C */
#define GPUEB_AUTO_DMA_0_R4                    (g_gpueb_cfgreg_base + 0x080)       /* 0x13C60080 */
#define GPUEB_AUTO_DMA_0_R5                    (g_gpueb_cfgreg_base + 0x084)       /* 0x13C60084 */
#define GPUEB_AUTO_DMA_0_R6                    (g_gpueb_cfgreg_base + 0x088)       /* 0x13C60088 */
#define GPUEB_AUTO_DMA_0_R7                    (g_gpueb_cfgreg_base + 0x08C)       /* 0x13C6008C */
#define GPUEB_AUTO_DMA_1_STATUS                (g_gpueb_cfgreg_base + 0x160)       /* 0x13C60160 */
#define GPUEB_AUTO_DMA_1_CUR_PC                (g_gpueb_cfgreg_base + 0x164)       /* 0x13C60164 */
#define GPUEB_AUTO_DMA_1_TRIGGER_STATUS        (g_gpueb_cfgreg_base + 0x16C)       /* 0x13C6016C */
#define GPUEB_AUTO_DMA_1_R0                    (g_gpueb_cfgreg_base + 0x170)       /* 0x13C60170 */
#define GPUEB_AUTO_DMA_1_R1                    (g_gpueb_cfgreg_base + 0x174)       /* 0x13C60174 */
#define GPUEB_AUTO_DMA_1_R2                    (g_gpueb_cfgreg_base + 0x178)       /* 0x13C60178 */
#define GPUEB_AUTO_DMA_1_R3                    (g_gpueb_cfgreg_base + 0x17C)       /* 0x13C6017C */
#define GPUEB_AUTO_DMA_1_R4                    (g_gpueb_cfgreg_base + 0x180)       /* 0x13C60180 */
#define GPUEB_AUTO_DMA_1_R5                    (g_gpueb_cfgreg_base + 0x184)       /* 0x13C60184 */
#define GPUEB_AUTO_DMA_1_R6                    (g_gpueb_cfgreg_base + 0x188)       /* 0x13C60188 */
#define GPUEB_AUTO_DMA_1_R7                    (g_gpueb_cfgreg_base + 0x18C)       /* 0x13C6018C */
#define GPUEB_CFGREG_SW_RSTN                   (g_gpueb_cfgreg_base + 0x600)       /* 0x13C60600 */

#define MCDI_MBOX_BASE                         (0x0C0DF7E0)
#define MCDI_MBOX_GPU_STA                      (g_mcdi_mbox_base + 0x008)          /* 0x0C0DF7E8 */

#define MCUSYS_PAR_WRAP_BASE                   (0x0C000000)
#define MCUSYS_PAR_WRAP_ACP_GALS_DBG           (g_mcusys_par_wrap_base + 0xB2C)    /* 0x0C000B2C */

#endif /* __GPUFREQ_REG_MT6989_H__ */
