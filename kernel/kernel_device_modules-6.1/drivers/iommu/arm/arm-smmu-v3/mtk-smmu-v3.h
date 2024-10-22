/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 */

#ifndef _MTK_SMMU_V3_H_
#define _MTK_SMMU_V3_H_

#include <linux/iommu.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <soc/mediatek/smi.h>

#include <dt-bindings/memory/mtk-memory-port.h>
#include "arm-smmu-v3.h"

#if (IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3) && \
	IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG))
#define MTK_SMMU_DEBUG		(1)
#endif

#define SMMU_TRANS_S1		(1 << 0)
#define SMMU_TRANS_S2		(1 << 1)
#define SMMU_TRANS_MIX		(SMMU_TRANS_S1 | SMMU_TRANS_S2)
#define SMMU_TRANS_PA		(1 << 2)
#define SMMU_TRANS_BYPASS	(SMMU_TRANS_PA | SMMU_TRANS_MIX)
#define SMMU_TRANS_WP_BYPASS	(SMMU_TRANS_PA | SMMU_TRANS_MIX | (1 << 3))

// all event ids.
#define EVENT_ID_UUT_FAULT		0x01
#define EVENT_ID_BAD_STREAMID_FAULT	0x02
#define EVENT_ID_STE_FETCH_FAULT	0x03
#define EVENT_ID_BAD_STE_FAULT		0x04
#define EVENT_ID_BAD_ATS_TREQ_FAULT	0x05
#define EVENT_ID_STREAM_DISABLED_FAULT	0x06
#define EVENT_ID_TRANSL_FORBIDDEN_FAULT	0x07
#define EVENT_ID_BAD_SUBSTREAMID_FAULT	0x08
#define EVENT_ID_CD_FETCH_FAULT		0x09
#define EVENT_ID_TRANSLATION_FAULT	0x10
#define EVENT_ID_ADDR_SIZE_FAULT	0x11
#define EVENT_ID_ACCESS_FAULT		0x12
#define EVENT_ID_PERMISSION_FAULT	0x13
#define EVENT_ID_BAD_CD_FAULT		0X0A
#define EVENT_ID_WALK_EABT_FAULT	0X0B
#define EVENT_ID_TLB_CONFLICT_FAULT	0X20
#define EVENT_ID_CFG_CONFLICT_FAULT	0X21
#define EVENT_ID_PAGE_REQUEST_FAULT	0X24
#define EVENT_ID_VMS_FETCH_FAULT	0X25

#define MTK_SMMU_HAS_FLAG(pdata, _x)	\
			  ((((pdata)->flags) & (_x)) == (_x))

//=====================================================
//Common macro definitions
//=====================================================
#define F_BIT_SET(bit)			(1<<(bit))
#define F_BIT_VAL(val, bit)		((!!(val))<<(bit))

//=====================================================
//SMMU wrapper register definition
//=====================================================
#define SMMU_TBU_CNT_MAX		(4)
#define SMMU_TBU_CNT(id)		((id) == SOC_SMMU ?	\
					 (SMMU_TBU_CNT_MAX - 1) : SMMU_TBU_CNT_MAX)

#define SMMUWP_CFG_OFFSET		(0x1ff000)

#define SMMUWP_GLB_CTL0			(0x0)
#define CTL0_STD_AXI_MODE_DIS		F_BIT_SET(0)
#define CTL0_MON_DIS			F_BIT_SET(1)
#define CTL0_DCM_EN			F_BIT_SET(2)
#define CTL0_WRAPPER_CK_AOEN		F_BIT_SET(3)
#define CTL0_AUTO_AXDOMAIN_EN		F_BIT_SET(4)
#define CTL0_IRQ_BUSY_EN		F_BIT_SET(5)
#define CTL0_ABT_CNT_CLR		F_BIT_SET(6)
#define CTL0_LEGACY_AXCACHE		F_BIT_SET(7)
#define CTL0_COMMIT_DIS			F_BIT_SET(8)
#define CTL0_AUTO_SLP_DIS		F_BIT_SET(9)
#define CTL0_STTSL_DIS			F_BIT_SET(10)
#define CTL0_CFG_TAB_DCM_EN		F_BIT_SET(11)
#define CTL0_CPU_PARTID_DIS		F_BIT_SET(14)

#define SMMUWP_GLB_CTL3			(0xc)
#define CTL3_BP_SMMU			F_BIT_SET(0)

#define SMMUWP_GLB_CTL4			(0x10)
#define CTL4_TBU_IRQ_SEL		GENMASK(1, 0)
#define CTL4_FENCE_TIMER		GENMASK(15, 8)
/* Latency water mark, used with SMMUWP_TCU_MON4, SMMUWP_TBUx_MON9, SMMUWP_TBUx_MON10 */
#define CTL4_LAT_SPEC			GENMASK(31, 16)
#define LAT_SPEC_DEF_VAL		(2400)

/*
 * Start LMU
 * 0: Snapshot LMU counter values, and stop LMU monitoring
 * 1: Snapshot LMU counter values, reset counters and keep recording
 */
#define SMMUWP_LMU_CTL0			(0x78)
#define CTL0_LAT_MON_START		F_BIT_SET(0)

#define SMMUWP_IRQ_NS_STA		(0x80)
#define STA_NS_TCU_GLB_INTR		F_BIT_SET(0)
#define STA_NS_TCU_CMD_SYNC_INTR	F_BIT_SET(1)
#define STA_NS_TCU_EVTQ_INTR		F_BIT_SET(2)
#define STA_TCU_PRI_INTR		F_BIT_SET(3)
#define STA_TCU_PMU_INTR		F_BIT_SET(4)
#define STA_TCU_RAS_CRI			F_BIT_SET(5)
#define STA_TCU_RAS_ERI			F_BIT_SET(6)
#define STA_TCU_RAS_FHI			F_BIT_SET(7)
/* SOC_SMMU is 3 TBU, others are 4 TBU */
#define STA_TBUx_RAS_CRI(tbu)		F_BIT_SET(8 + tbu)
#define STA_TBUx_RAS_ERI(tbu)		F_BIT_SET(12 + tbu)
#define STA_TBUx_RAS_FHI(tbu)		F_BIT_SET(16 + tbu)

#define SMMUWP_IRQ_NS_ACK		(0x84)

#define SMMUWP_IRQ_NS_ACK_CNT		(0x88)
#define IRQ_NS_ACK_CNT_MSK		GENMASK(7, 0)


/* SMMU non-secure interrupt pending count register, count 20 */
#define SMMUWP_IRQ_NS_CNTx(cnt)		(0x100 + 0x4 * (cnt))

#define SMMUWP_IRQ_S_STA		(0x90)
#define STA_S_TCU_GLB_INTR		F_BIT_SET(0)
#define STA_S_TCU_CMD_SYNC_INTR		F_BIT_SET(1)
#define STA_S_TCU_EVTQ_INTR		F_BIT_SET(2)

#define SMMUWP_IRQ_S_ACK		(0x94)

#define SMMUWP_IRQ_S_ACK_CNT		(0x98)
#define IRQ_S_ACK_CNT_MSK		GENMASK(7, 0)

/* SMMU secure interrupt pending count register, count 3 */
#define SMMUWP_IRQ_S_CNTx(cnt)		(0x180 + 0x4 * (cnt))

#define SMMU_TCU_CTL1_AXSLC		(0x204)
#define AXSLC_BIT_FIELD			GENMASK(8, 4)
#define AXSLC_CACHE			F_BIT_SET(5)
#define AXSLC_ALLOCATE			F_BIT_SET(6)
#define AXSLC_SPECULATIVE		F_BIT_SET(7)
#define AXSLC_SET			(AXSLC_CACHE | AXSLC_ALLOCATE | AXSLC_SPECULATIVE)
#define SLC_SB_ONLY_EN			F_BIT_SET(1)
/*
 * TCU_DVM_EN_REQ: Set 1 for connecting to DVM
 * TCU_DVM_EN_ACK: Poll 1 to wait for ACK connected to DVM
 */
#define SMMUWP_TCU_CTL4			(0x210)
#define TCU_DVM_EN_REQ			F_BIT_SET(0)
#define TCU_DVM_EN_ACK			F_BIT_SET(1)

#define SMMUWP_POLL_DVM_TIMEOUT_US	(1000) /* 1ms */

/* SMMU TCU latency meters control registers
 * TCU_MON_ID: The monitoring AXI ID if needed, default monitor all
 * TCU_MON_DIS: 0:Enable measure, 1:Disable measure. Default 0
 * TCU_MON_ID_MASK: Used with TCU_MON_ID, if TCU_MON_ID is specified, all bits of TCU_MON_ID_MASK
 * should be all 1
 */
#define SMMUWP_TCU_CTL8			(0x220)
#define TCU_MON_ID			GENMASK(14, 0)
#define TCU_MON_DIS			F_BIT_SET(15)
#define TCU_MON_ID_MASK			GENMASK(30, 16)

/*
 * SMMU TCU MON1/2/3 are used to measure latency, related control register: SMMUWP_TCU_CTL8
 * Only measure read command, donot measure write command.
 * Default disabled, which works like a stopwatch.
 * Start counting via set SMMUWP_LMU_CTL0[0]=1. End counting via set SMMUWP_LMU_CTL0[0]=0.
 * Average command latency = TCU_LAT_TOT/TCU_TRANS_TOT
 */
/*
 * TCU_LAT_MAX: Maximum latency(T) from TCU`s read command to last data
 * TCU_PEND_MAX: Maximum pending latency(pended by EMI, arvalid_m=1 and arready_m=0)
 */
#define SMMUWP_TCU_MON1			(0x244)
#define TCU_LAT_MAX			GENMASK(15, 0)
#define TCU_PEND_MAX			GENMASK(31, 16)

/* TCU_LAT_TOT, sum of latency of total read commands */
#define SMMUWP_TCU_MON2			(0x248)

/* TCU_TRANS_TOT, total read command count */
#define SMMUWP_TCU_MON3			(0x24c)

/* TCU_OOS_TRANS_TOT, total read command count, which over latency water mark(CTL4_LAT_SPEC)  */
#define SMMUWP_TCU_MON4			(0x250)

/*
 * SMMU TCU MON5/6/7 are default enabled,
 * Set SMMUWP_GLB_CTL0[1] to 1->0 for clear counting.
 */
/* SMMU TCU send read command total */
#define SMMUWP_TCU_MON5			(0x254)

/* SMMU TCU send write command total */
#define SMMUWP_TCU_MON6			(0x258)

/* SMMU TCU receive DVM command total */
#define SMMUWP_TCU_MON7			(0x25c)

#define SMMUWP_TCU_DBG0			(0x280)
#define TCU_AXI_DBG_M			GENMASK(15, 0)

#define SMMUWP_TCU_DBG1			(0x284)
#define TCU_AROSTD_M			GENMASK(15, 0)
#define TCU_AWOSTD_M			GENMASK(31, 16)

#define SMMUWP_TCU_DBG2			(0x288)
#define TCU_WOSTD_M			GENMASK(15, 0)
#define TCU_WDAT_M			GENMASK(31, 16)

#define SMMUWP_TCU_DBG3			(0x28c)
#define TCU_RDAT_M			GENMASK(15, 0)
#define TCU_ACOSTD_M			GENMASK(31, 16)

#define SMMUWP_TCU_DBG4			(0x290)
#define TCU_DVM_AC_OSTD			GENMASK(7, 0)
#define TCU_DVM_SYNC_OSTD		GENMASK(15, 8)
#define TCU_DVM_COMP_OSTD		GENMASK(23, 16)

/* SMMU TBUx latency meters control registers */
/*
 * RLAT_MON_ID: AXI ID to monitor for read commands, default monitor all
 * RLAT_MON_DIS: 0:Enable read monitor, 1:Disable read monitor. Default 0
 */
#define SMMUWP_TBUx_CTL4(tbu)		(0x310 + 0x100 * (tbu))
#define CTL4_RLAT_MON_ID		GENMASK(15, 0)
#define CTL4_RLAT_MON_DIS		F_BIT_SET(31)

/*
 * WLAT_MON_ID: AXI ID to monitor for write commands, default monitor all
 * WLAT_MON_DIS: 0:Enable write monitor, 1:Disable write monitor. Default 0
 */
#define SMMUWP_TBUx_CTL5(tbu)		(0x314 + 0x100 * (tbu))
#define CTL5_WLAT_MON_ID		GENMASK(15, 0)
#define CTL5_WLAT_MON_DIS		F_BIT_SET(31)

/*
 * RLAT_MON_ID_MASK: Used with RLAT_MON_ID, if RLAT_MON_ID is specified, all bits of
 * RLAT_MON_ID_MASK should be all 1
 */
#define SMMUWP_TBUx_CTL6(tbu)		(0x318 + 0x100 * (tbu))
#define CTL6_RLAT_MON_ID_MASK		GENMASK(15, 0)

/*
 * WLAT_MON_ID_MASK: Used with WLAT_MON_ID, if WLAT_MON_ID is specified, all bits of
 * WLAT_MON_ID_MASK should be all 1
 */
#define SMMUWP_TBUx_CTL7(tbu)		(0x31c + 0x100 * (tbu))
#define CTL7_WLAT_MON_ID_MASK		GENMASK(15, 0)

/*
 * SMMU TBU MON1~10 are used to measure latency.
 * Default disabled, which works like a stopwatch.
 * Start counting via set SMMUWP_LMU_CTL0[0]=1. End counting via set SMMUWP_LMU_CTL0[0]=0.
 * Then read the count of TBU MON during start and end.
 * Average command latency = RLAT_TOT/RTRANS_TOT.
 */
/*
 * RLAT_MAX: maximum latency(T) of TBUx read commands from SMMU`s input to SMMU`s output
 * WLAT_MAX: maximum latency(T) of TBUx write commands from SMMU`s input to SMMU`s output
 */
#define SMMUWP_TBUx_MON1(tbu)		(0x334 + 0x100 * (tbu))
#define MON1_RLAT_MAX			GENMASK(15, 0)
#define MON1_WLAT_MAX			GENMASK(31, 16)

/* ID_RLAT_MAX, the AXI ID of the maximum latency of TBUx read command */
#define SMMUWP_TBUx_MON2(tbu)		(0x338 + 0x100 * (tbu))
#define MON2_ID_RLAT_MAX		GENMASK(19, 0)

/* ID_WLAT_MAX, the AXI ID of the maximum latency of TBUx write command */
#define SMMUWP_TBUx_MON3(tbu)		(0x33c + 0x100 * (tbu))
#define MON3_ID_WLAT_MAX		GENMASK(19, 0)

/*
 * Maximum pending latency(pended by EMI, arvalid_m=1 and arready_m=0)
 * RPEND_MAX: the maximum pending latency of TBUx read command
 * WPEND_MAX: the maximum pending latency of TBUx write command
 */
#define SMMUWP_TBUx_MON4(tbu)		(0x340 + 0x100 * (tbu))
#define MON4_RPEND_MAX			GENMASK(15, 0)
#define MON4_WPEND_MAX			GENMASK(31, 16)

/* RLAT_TOT, sum of latency of total TBUx read commands */
#define SMMUWP_TBUx_MON5(tbu)		(0x344 + 0x100 * (tbu))

/* WLAT_TOT, sum of latency of total TBUx write commands */
#define SMMUWP_TBUx_MON6(tbu)		(0x348 + 0x100 * (tbu))

/* RTRANS_TOT, total TBUx read command count */
#define SMMUWP_TBUx_MON7(tbu)		(0x34C + 0x100 * (tbu))

/* WTRANS_TOT, total TBUx write command count */
#define SMMUWP_TBUx_MON8(tbu)		(0x350 + 0x100 * (tbu))

/*
 * ROOS_TRANS_TOT, SMMU TBUx read command translation total,
 * which over latency water mark(CTL4_LAT_SPEC)
 */
#define SMMUWP_TBUx_MON9(tbu)		(0x354 + 0x100 * (tbu))

/*
 * WOOS_TRANS_TOT, SMMU TBUx write command translation total,
 * which over latency water mark(CTL4_LAT_SPEC)
 */
#define SMMUWP_TBUx_MON10(tbu)		(0x358 + 0x100 * (tbu))

#define SMMUWP_TBUx_DBG0(tbu)		(0x360 + 0x100 * (tbu))
#define DBG0_AXI_DBG_S			GENMASK(15, 0)
#define DBG0_AXI_DBG_M			GENMASK(31, 16)

#define SMMUWP_TBUx_DBG1(tbu)		(0x364 + 0x100 * (tbu))
#define DBG1_AWOSTD_S			GENMASK(15, 0)
#define DBG1_AWOSTD_M			GENMASK(31, 16)

#define SMMUWP_TBUx_DBG2(tbu)		(0x368 + 0x100 * (tbu))
#define DBG2_AROSTD_S			GENMASK(15, 0)
#define DBG2_AROSTD_M			GENMASK(31, 16)

#define SMMUWP_TBUx_DBG3(tbu)		(0x36c + 0x100 * (tbu))
#define DBG3_WOSTD_S			GENMASK(15, 0)
#define DBG3_WOSTD_M			GENMASK(31, 16)

#define SMMUWP_TBUx_DBG4(tbu)		(0x370 + 0x100 * (tbu))
#define DBG4_WDAT_S			GENMASK(15, 0)
#define DBG4_WDAT_M			GENMASK(31, 16)

#define SMMUWP_TBUx_DBG5(tbu)		(0x374 + 0x100 * (tbu))
#define DBG5_RDAT_S			GENMASK(15, 0)
#define DBG5_RDAT_M			GENMASK(31, 16)

/* SMMU TBUx read translation fault monitor0 */
#define SMMUWP_TBUx_RTFM0(tbu)		(0x380 + 0x100 * (tbu))
#define RTFM0_FAULT_AXI_ID		GENMASK_ULL(19, 0)
#define RTFM0_FAULT_DET			F_BIT_SET(31)

/* SMMU TBUx read translation fault monitor1 */
#define SMMUWP_TBUx_RTFM1(tbu)		(0x384 + 0x100 * (tbu))
#define RTFM1_FAULT_VA_35_32		GENMASK_ULL(3, 0)
#define RTFM1_FAULT_VA_31_12		GENMASK_ULL(31, 12)

/* SMMU TBUx read translation fault monitor2 */
#define SMMUWP_TBUx_RTFM2(tbu)		(0x388 + 0x100 * (tbu))
#define RTFM2_FAULT_SID			GENMASK_ULL(7, 0)
#define RTFM2_FAULT_SSID		GENMASK_ULL(15, 8)
#define RTFM2_FAULT_SSIDV		F_BIT_SET(16)
#define RTFM2_FAULT_SECSID		F_BIT_SET(17)

/* SMMU TBUx write translation fault monitor0 */
#define SMMUWP_TBUx_WTFM0(tbu)		(0x390 + 0x100 * (tbu))
#define WTFM0_FAULT_AXI_ID		GENMASK_ULL(19, 0)
#define WTFM0_FAULT_DET			F_BIT_SET(31)

/* SMMU TBUx write translation fault monitor1 */
#define SMMUWP_TBUx_WTFM1(tbu)		(0x394 + 0x100 * (tbu))
#define WTFM1_FAULT_VA_35_32		GENMASK_ULL(3, 0)
#define WTFM1_FAULT_VA_31_12		GENMASK_ULL(31, 12)

/* SMMU TBUx write translation fault monitor2 */
#define SMMUWP_TBUx_WTFM2(tbu)		(0x398 + 0x100 * (tbu))
#define WTFM2_FAULT_SID			GENMASK_ULL(7, 0)
#define WTFM2_FAULT_SSID		GENMASK_ULL(15, 8)
#define WTFM2_FAULT_SSIDV		F_BIT_SET(16)
#define WTFM2_FAULT_SECSID		F_BIT_SET(17)

/* SMMU TBU Manual OG Control High Register0 */
#define SMMUWP_TBU0_MOGH0		(0x3b4)
#define MOGH_EN				F_BIT_SET(29)
#define MOGH_RW				F_BIT_SET(28)

/* SMMU translation fault TBUx */
#define SMMUWP_TF_MSK			GENMASK(23, 0)
#define SMMUWP_TF_TBU_MSK		GENMASK(26, 24)
#define SMMUWP_TF_TBU(tbu)		FIELD_PREP(SMMUWP_TF_TBU_MSK, tbu)
#define SMMUWP_TF_TBU_VAL(id)		FIELD_GET(SMMUWP_TF_TBU_MSK, id)

/* SMMU MPAM */
#define SMMU_MPAM_TCU_OFFSET		0x3000
#define SMMU_MPAM_TBUx_OFFSET(tbu)	(0x43000 + 0x20000 * (tbu))
#define SMMU_MPAM_REG_SZ		0x1000

#define MPAMF_IDR_LO			0x0
#define MPAMF_IDR_LO_PARTID_MAX		GENMASK(15, 0)
#define MPAMF_IDR_LO_PMG_MAX		GENMASK(23, 16)
#define MPAMF_IDR_LO_HAS_CCAP_PART	BIT(24)
#define MPAMF_IDR_LO_HAS_MSMON		BIT(30)

#define MPAMF_IDR_HI			0x4
#define MPAMF_IDR_HI_HAS_RIS		BIT(0)
#define MPAMF_IDR_HI_RIS_MAX		GENMASK(27, 24)

#define MPAMF_CCAP_IDR			0x38
#define CCAP_IDR_CMAX_WD		GENMASK(5, 0)
#define CCAP_IDR_HAS_CMAX_SOFTLIM	BIT(31)

#define MPAMF_CSUMON_IDR		0x88
#define CSUMON_IDR_NUM_MON		GENMASK(15, 0)
#define CSUMON_IDR_HAS_CAPTURE		BIT(31)

#define MPAMCFG_PART_SEL		0x0100
#define PART_SEL_PARTID_SEL		GENMASK(15, 0)
#define PART_SEL_INTERNAL		BIT(16)
#define PART_SEL_RIS			GENMASK(27, 24)

#define MPAMCFG_CMAX			0x0108
#define MPAMCFG_CMAX_CMAX		GENMASK(15, 8)
#define MPAMCFG_CMAX_SOFTLIM		BIT(31)

#define MSMON_CFG_MON_SEL		0x800
#define CFG_MON_SEL_MON_SEL		GENMASK(1, 0)
#define CFG_MON_SEL_RIS			GENMASK(27, 24)

#define MSMON_CFG_CSU_FLT		0x810
#define CSU_FLT_PARTID			GENMASK(15, 0)
#define CSU_FLT_PMG			GENMASK(23, 16)

#define MSMON_CFG_CSU_CTL		0x818
#define CSU_CTL_MATCH_PARTID		BIT(16)
#define CSU_CTL_MATCH_PMG		BIT(17)
#define CSU_CTL_EN			BIT(31)

#define MSMON_CSU			0x840
#define MSMON_CSU_VALUE			GENMASK(30, 0)
#define MSMON_NRDY			BIT(31)

/*
 * HYP SMMU action list
 * 1. INVALID_BIT is used to distinguish smc ret val, which is valid or not.
 * 2. HYP SMMU events may conflict with original SMMU event id, therefore,
 * Adding HYP_SMMU_EVENT_BASE can avoid this issue.
 * 3. Using HYP_PMM_SMMU_DEBUG_PARA to set up hyp smmu smc's parameters.
 * The parameters lay is descibed in the blelow
 * sid		[7:0]
 * smmu type	[9:8]
 * reg		[18:10]
 * ste row	[26:19]
 * action id	[31:27]
 */
#define HYP_SMMU_INFO_PREFIX "HYP_SMMU"
#define HYP_SMMU_INVALID_SID_BIT	(1UL << 63)
#define HYP_SMMU_INVALID_VMID_BIT	(1UL << 62)
#define HYP_SMMU_INVALID_IPA_BIT	(1UL << 61)
#define HYP_SMMU_INVALID_STE_ROW_BIT	(1UL << 60)
#define HYP_SMMU_INVALID_ACTION_ID_BIT	(1UL << 59)
#define HYP_SMMU_INVALID_SMMU_TYPE_BIT	(1UL << 58)
#define HYP_SMMU_EVENT_BASE		BIT(8)
#define HYP_SMMU_REG_DUMP_EVT		(HYP_SMMU_EVENT_BASE | 0x1)
#define HYP_SMMU_GLOBAL_STE_DUMP_EVT	(HYP_SMMU_EVENT_BASE | 0x2)
#define HYP_PMM_SMMU_DEBUG_PARA(action_id, ste_row, reg, smu_type, sid)        \
	((action_id << 27) | (ste_row << 19) | (reg << 10) | (smu_type << 8) | \
	 sid)

enum mtk_smmu_type {
	MM_SMMU,
	APU_SMMU,
	SOC_SMMU,
	GPU_SMMU,
	SMMU_TYPE_NUM
};

enum mm_smmu {
	MM_SMMU_MDP,
	MM_SMMU_DISP,
	MM_SMMU_NUM
};

enum apu_smmu {
	APU_SMMU_M0,
	APU_SMMU_NUM
};

enum soc_smmu {
	SOC_SMMU_M4,
	SOC_SMMU_M6,
	SOC_SMMU_M7,
	SOC_SMMU_NUM
};

enum mtk_smmu_plat {
	SMMU_MT6989,
};

struct smmuv3_pmu_impl {
	int (*irq_set_up)(int irq, struct device *dev);
	irqreturn_t (*pmu_irq_handler)(int irq, struct device *dev);
	int (*late_init)(void __iomem *reg_base, struct device *dev);
};

struct smmuv3_pmu_device {
	struct device			*dev;
	const struct smmuv3_pmu_impl	*impl;
	struct list_head		node;
};

struct iommu_group_data {
	struct iommu_group		*iommu_group;
	enum mtk_smmu_type		smmu_type;
	void				*group_id;
	unsigned int			domain_type;
	const char			*label;
};

struct mtk_smmu_plat_data {
	enum mtk_smmu_plat		smmu_plat;
	u32				flags;
	enum mtk_smmu_type		smmu_type;
};

struct txu_mpam_data {
	void __iomem			*mpam_base;
	u32				cmax_max_int;
	bool				has_cmax_softlim;
	u16				partid_max;
	u8				pmg_max;
	bool				has_ccap_part;
	bool				has_msmon;
	bool				has_ris;
	u32				ris_max;
};

struct mtk_pm_ops {
	int (*pm_get)(void);
	int (*pm_put)(void);
};

struct mtk_smmu_data {
	const struct mtk_smmu_plat_data *plat_data;
	const struct mtk_pm_ops		*pm_ops;
	struct mtk_smi_larb_iommu	larb_imu[MTK_LARB_NR_MAX];
	struct mutex			group_mutexs;
	struct arm_smmu_device		smmu;
	struct iommu_group_data		*iommu_groups;
	spinlock_t			pmu_lock;
	struct list_head		pmu_devices;
	u32				iommu_group_nr;
	u32				smmu_trans_type;
	u32				hw_init_flag;
	struct txu_mpam_data		*txu_mpam_data;
	u32				txu_mpam_data_cnt;
	u32				partid_max;
	u32				pmg_max;
	u32				tcu_prefetch;
	bool				axslc;
	u32				*smi_com_base;
	u32				smi_com_base_cnt;
	u32				irq_cnt;
	unsigned long			irq_first_jiffies;
	struct timer_list		irq_pause_timer;
};

enum mtk_smmu_tfm_type {
	SMMU_TFM_READ,
	SMMU_TFM_WRITE,
	SMMU_TFM_TYPE_NUM
};

struct mtk_smmu_fault_param {
	enum mtk_smmu_tfm_type tfm_type;
	bool fault_det;
	u64 fault_iova;
	u64 fault_pa;
	u32 fault_id;
	u32 fault_sid;
	u32 fault_ssid;
	u32 fault_ssidv;
	u32 fault_secsid;
	u32 tbu_id;
	u32 larb_id;
	u32 port_id;
	const char *port_name;
};

struct mtk_iommu_fault_event {
	struct mtk_smmu_fault_param mtk_fault_param[SMMU_TFM_TYPE_NUM][SMMU_TBU_CNT_MAX];
	struct iommu_fault_event fault_evt;
};

struct mtk_smmu_ops {
	struct mtk_smmu_data* (*get_smmu_data)(u32 smmu_type);
	__le64* (*get_cd_ptr)(struct arm_smmu_domain *smmu_domain, u32 ssid);
	__le64* (*get_step_ptr)(struct arm_smmu_device *smmu, u32 sid);
	int (*smmu_power_get)(struct arm_smmu_device *smmu);
	int (*smmu_power_put)(struct arm_smmu_device *smmu);
};

static inline bool smmu_v3_enabled(void)
{
	struct device_node *node = NULL;
	static int smmuv3_enable = -1;

	if (!IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3))
		return false;

	if (smmuv3_enable == -1) {
		node = of_find_compatible_node(NULL, NULL, "arm,smmu-v3");
		smmuv3_enable = (node != NULL ? 1 : 0);
	}

	return (smmuv3_enable == 1);
}

static inline struct mtk_smmu_data *to_mtk_smmu_data(struct arm_smmu_device *smmu)
{
	return container_of(smmu, struct mtk_smmu_data, smmu);
}

static inline struct arm_smmu_device *get_smmu_device(struct device *dev)
{
	struct arm_smmu_master *master;

	if (!dev)
		return NULL;

	master = dev_iommu_priv_get(dev);
	if (!master || !master->smmu)
		return NULL;

	return master->smmu;
}

static inline int get_smmu_id(struct device *dev)
{
	struct arm_smmu_device *smmu;
	struct mtk_smmu_data *data;

	smmu = get_smmu_device(dev);
	if (!smmu)
		return 0;

	data = to_mtk_smmu_data(smmu);
	return data->plat_data->smmu_type;
}

static inline int get_smmu_stream_id(struct device *dev)
{
	struct arm_smmu_master *master;

	if (!dev)
		return 0;

	master = dev_iommu_priv_get(dev);
	if (!master || !master->streams)
		return 0;

	return master->streams[0].id;
}

static inline int get_smmu_asid(struct device *dev)
{
	struct iommu_domain *domain;
	struct arm_smmu_domain *smmu_domain;

	if (!dev)
		return 0;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain)
		return 0;

	smmu_domain = container_of(domain, struct arm_smmu_domain, domain);

	return smmu_domain->s1_cfg.cd.asid;
}

static inline u64 get_smmu_tab_id(struct device *dev)
{
	u64 smmu_id, asid;

	smmu_id = get_smmu_id(dev);
	asid = get_smmu_asid(dev);

	return smmu_id << 32 | asid;
}

static inline int smmu_tab_id_to_smmu_id(u64 tab_id)
{
	return tab_id >> 32;
}

static inline int smmu_tab_id_to_asid(u64 tab_id)
{
	return tab_id & 0xffffffff;
}

static inline struct device *mtk_smmu_get_shared_device(struct device *dev)
{
	struct device_node *node;
	struct platform_device *shared_pdev;
	struct device *shared_dev = dev;

	if (!smmu_v3_enabled())
		return dev;

	node = of_parse_phandle(dev->of_node, "mtk,smmu-shared", 0);
	if (node) {
		shared_pdev = of_find_device_by_node(node);
		if (shared_pdev)
			shared_dev = &shared_pdev->dev;
	}

	return shared_dev;
}

static inline struct mtk_smmu_data *get_smmu_data(u32 smmu_type)
{
	struct platform_device *smmu_pdev;
	struct device_node *smmu_node;
	struct arm_smmu_device *smmu;
	struct mtk_smmu_data *data;

	if (!IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3))
		return NULL;

	if (smmu_type >= SMMU_TYPE_NUM) {
		pr_info("%s invalid smmu_type %u\n", __func__, smmu_type);
		return NULL;
	}

	for_each_compatible_node(smmu_node, NULL, "arm,smmu-v3") {
		smmu_pdev = of_find_device_by_node(smmu_node);
		of_node_put(smmu_node);
		if (!smmu_pdev) {
			pr_info("%s, can't find smmu pdev\n", __func__);
			return NULL;
		}

		smmu = platform_get_drvdata(smmu_pdev);
		if (!smmu) {
			pr_info("%s, can't find smmu device\n", __func__);
			return NULL;
		}

		data = to_mtk_smmu_data(smmu);
		if (smmu_type == data->plat_data->smmu_type)
			return data;
	}

	return NULL;
}

static inline int mtk_smmu_set_pm_ops(u32 smmu_type, const struct mtk_pm_ops *ops)
{
	struct mtk_smmu_data *data = get_smmu_data(smmu_type);

	if (data == NULL) {
		pr_info("%s can't find smmu data, smmu_type %u\n", __func__, smmu_type);
		return -1;
	}

	data->pm_ops = ops;

	return 0;
}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3) && IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
void mtk_smmu_reg_dump(enum mtk_smmu_type type,
		       struct device *master_dev,
		       int sid);
int mtk_smmu_tf_detect(enum mtk_smmu_type type,
		       struct device *master_dev,
		       u32 sid, u32 tbu,
		       u32 *axids, u32 num_axids,
		       struct mtk_smmu_fault_param *param);

int mtk_smmu_start_transaction_counter(struct device *dev);
void mtk_smmu_end_transaction_counter(struct device *dev,
				      unsigned long *tcu_tot,
				      unsigned long *tbu_tot);
int mtk_smmu_start_latency_monitor(struct device *dev,
				   int mon_axiid,
				   int lat_spec);
void mtk_smmu_end_latency_monitor(struct device *dev,
				  unsigned int *maxlat_axiid,
				  unsigned long *tcu_rlat_tots,
				  unsigned long *tbu_lat_tots,
				  unsigned long *oos_trans_tot);
void mtk_smmu_dump_outstanding_monitor(struct device *dev);
void mtk_smmu_dump_io_interface_signals(struct device *dev);
void mtk_smmu_dump_dcm_en(struct device *dev);
int mtk_smmu_register_pmu_device(struct smmuv3_pmu_device *pmu_device);
void mtk_smmu_unregister_pmu_device(struct smmuv3_pmu_device *pmu_device);
#else
static inline void mtk_smmu_reg_dump(enum mtk_smmu_type type,
				     struct device *master_dev,
				     int sid)
{
}

static inline int mtk_smmu_tf_detect(enum mtk_smmu_type type,
				     struct device *master_dev,
				     u32 sid, u32 tbu,
				     u32 *axids, u32 num_axids,
				     struct mtk_smmu_fault_param *param)
{
	return 0;
}

static inline int mtk_smmu_start_transaction_counter(struct device *dev)
{
	return 0;
}

static inline void mtk_smmu_end_transaction_counter(struct device *dev,
						    unsigned long *tcu_tot,
						    unsigned long *tbu_tot)
{
}

static inline int mtk_smmu_start_latency_monitor(struct device *dev,
						 int mon_axiid,
						 int lat_spec)
{
	return 0;
}

static inline void mtk_smmu_end_latency_monitor(struct device *dev,
						unsigned int *maxlat_axiid,
						unsigned long *tcu_rlat_tots,
						unsigned long *tbu_lat_tots,
						unsigned long *oos_trans_tot)
{
}

static inline void mtk_smmu_dump_outstanding_monitor(struct device *dev)
{
}

static inline void mtk_smmu_dump_io_interface_signals(struct device *dev)
{
}

static inline void mtk_smmu_dump_dcm_en(struct device *dev)
{
}

static inline int mtk_smmu_register_pmu_device(struct smmuv3_pmu_device *pmu_device)
{
	return 0;
}

static inline void mtk_smmu_unregister_pmu_device(struct smmuv3_pmu_device *pmu_device)
{
}
#endif /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
#endif /* _MTK_SMMU_V3_H_ */
