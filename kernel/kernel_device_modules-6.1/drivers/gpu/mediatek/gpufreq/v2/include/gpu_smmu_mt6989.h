/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#ifndef __GPUSMMU_MT6989_H__
#define __GPUSMMU_MT6989_H__
/* Private Header */

#define SMMU_LOGD_ENABLE 0
#define gpu_smmu_pr_err(fmt, args...) pr_err("[GPU/SMMU][E]%s: "fmt"\n", __func__, ##args)
#define gpu_smmu_pr_info(fmt, args...) pr_info("[GPU/SMMU][I]@%s: "fmt"\n", __func__, ##args)
#if SMMU_LOGD_ENABLE
#define gpu_smmu_pr_debug(fmt, args...) pr_debug("[GPU/SMMU][D]%s: "fmt"\n", __func__, ##args)
#else
#define gpu_smmu_pr_debug(fmt, args...)
#endif

/* MFG-SMMU */
#define ENABLE_MFG_SMMU_SPM_SEMA   1
#define ENABLE_MFG_SMMU_PWR_CTRL   1
/* Debug */
#define ENABLE_MFG_SMMU_FOOTPRINT  0
#define ENABLE_MFG_SMMU_ON_EB      0
#define ENABLE_MFG_SMMU_REQ_SPMRSC 0

//#define MALI_AXUSER_BASE 0x13FC0000 // AP view
//#define MALI_AXUSER_BASE 0x02FC0000 // EB view
#define MALI_AXUSER_BASE g_mfg_top_base
//TODO: 0x02FCxxxx ? 0x03FCxxxx is fast path; 0x13FCxxxx is slow path

// g_sleep 0x1C001000
//#define SPM_SEMA_BASE 0x1c001000 // AP view
//#define SPM_SEMA_BASE 0x2c001000 // EB view
#define SPM_SEMA_M3_OFFSET 0x000006A8

// APB Module ifrbus_ao
//#define IFRBUS_AO_BASE 0x1002C000 //g_ifrbus_ao_base
#define EMISYS_PROTECT_EN_W1C_1	    (g_ifrbus_ao_base+0x128)
#define EMISYS_PROTECT_EN_W1C_0	    (g_ifrbus_ao_base+0x108)
#define EMISYS_PROTECT_RDY_STA_0	(g_ifrbus_ao_base+0x10c)
#define EMISYS_PROTECT_RDY_STA_1	(g_ifrbus_ao_base+0x12c)
#define EMISYS_PROTECT_EN_STA_0	    (g_ifrbus_ao_base+0x100)
#define EMISYS_PROTECT_EN_STA_1	    (g_ifrbus_ao_base+0x120)

//#define MFG_RPC_BASE 0x13F90000 //g_mfg_rpc_base
#define MFG_RPC_SLP_PROT_EN0_SET	(g_mfg_rpc_base+0x0080)
#define MFG_RPC_SLP_PROT_EN0_CLR	(g_mfg_rpc_base+0x0084)
#define MFG_RPC_SLP_PROT_EN1_SET	(g_mfg_rpc_base+0x0088)
#define MFG_RPC_SLP_PROT_EN1_CLR	(g_mfg_rpc_base+0x008c)
#define MFG_RPC_SLP_PROT_RDY_STA0	(g_mfg_rpc_base+0x0090)
#define MFG_RPC_SLP_PROT_RDY_STA1	(g_mfg_rpc_base+0x0094)
//#define MFG_RPC_MFG37_PWR_CON       (g_mfg_rpc_base+0x162c) //13F9162C
#define MFG_SODI_DDREN              (g_mfg_rpc_base+0x1700)
#define MFG_SODI_APSRC              (g_mfg_rpc_base+0x1704)
#define MFG_SODI_EMI                (g_mfg_rpc_base+0x1708)

/* Polling type */
#define POLL_TYPE_NOT_EQ 0 // polling while (read register != target value)
#define POLL_TYPE_IS_EQ  1 // polling while (read register == target value)
#define POLL_REG_TIMEOUT 1
#define POLL_REG_OK      0

/* SMMU Wrapper */
#define SMMU_GLB_CTL0 (g_mfg_smmu_base + 0x001ff000) /* 0x13BFF000 */
#define SMMU_GLB_CTL1 (g_mfg_smmu_base + 0x001ff004) /* 0x13BFF004 */
#define SMMU_TCU_CTL1 (g_mfg_smmu_base + 0x001ff204) /* 0x13BFF204 */
#define SMMU_TCU_CTL4 (g_mfg_smmu_base + 0x001ff210) /* 0x13BFF210 */

//#define GPUEB_SRAM_BASE         (0x13C00000)
#define GPUEB_SRAM_GPR15        (g_gpueb_sram_base + 0x4FD58) /* 0x13C4FD58 */

#ifndef BIT
#define BIT(nr)                         (1U << (nr))
#endif
#ifndef GENMASK
#define GENMASK(h, l)                   ((0xFFFFFFFF << (l)) & (0xFFFFFFFF >> (32 - 1 - (h))))
#endif

#endif /* __GPUSMMU_MT6989_H__ */
