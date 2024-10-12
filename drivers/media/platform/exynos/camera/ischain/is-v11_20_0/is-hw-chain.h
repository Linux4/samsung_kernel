// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v10.0 specific functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_CHAIN_H
#define IS_HW_CHAIN_H

#include "is-hw-config.h"
#include "pablo-hw-api-common.h"
#include "is-groupmgr.h"
#include "is-config.h"

enum sysreg_csis_reg_name {
	SYSREG_R_CSIS_MIPI_PHY_CON,
	SYSREG_CSIS_REG_CNT
};

enum sysreg_csis_reg_field {
	SYSREG_F_CSIS_MIPI_DPHY_CONFIG,
	SYSREG_F_CSIS_MIPI_RESETN_DPHY_S5,
	SYSREG_F_CSIS_MIPI_RESETN_DPHY_S4,
	SYSREG_F_CSIS_MIPI_RESETN_DPHY_S1,
	SYSREG_F_CSIS_MIPI_RESETN_DPHY_S,
	SYSREG_F_CSIS_MIPI_RESETN_DCPHY_S1,
	SYSREG_F_CSIS_MIPI_RESETN_DCPHY_S,
	SYSREG_CSIS_REG_FIELD_CNT
};

#define GROUP_HW_MAX (GROUP_SLOT_MAX)

#define IORESOURCE_CSTAT0	0
#define IORESOURCE_CSTAT1	1
#define IORESOURCE_CSTAT2	2
#define IORESOURCE_LME		3
#define IORESOURCE_BYRP		4
#define IORESOURCE_RGBP		5
#define IORESOURCE_MCFP		6
#define IORESOURCE_YUVP		7
#define IORESOURCE_MCSC		8

#define GROUP_SENSOR_MAX_WIDTH	16376
#define GROUP_SENSOR_MAX_HEIGHT	12288
#define GROUP_PDP_MAX_WIDTH	16376
#define GROUP_PDP_MAX_HEIGHT	12288
#define GROUP_3AA_MAX_WIDTH	16376
#define GROUP_3AA_MAX_HEIGHT	12288
#define GROUP_ITP_MAX_WIDTH	7680
#define GROUP_ITP_MAX_HEIGHT	4320
#define GROUP_VRA_MAX_WIDTH	640
#define GROUP_VRA_MAX_HEIGHT	480
#define GROUP_CLAHE_MAX_WIDTH	12000
#define GROUP_CLAHE_MAX_HEIGHT	9000
#define GROUP_LME_MAX_WIDTH	2016
#define GROUP_LME_MAX_HEIGHT	1512
#define GROUP_MCFP_MAX_WIDTH	4880
#define GROUP_MCFP_MAX_HEIGHT	4320
#define GROUP_BYRP_MAX_WIDTH	4880
#define GROUP_BYRP_MAX_HEIGHT	9000

/* RTA HEAP: 6MB */
#define IS_RESERVE_LIB_SIZE	(0x00600000)

/* ORBMCH DMA: Moved to user space */
#define TAAISP_ORBMCH_SIZE	(0)

/* DDK DMA: Moved into driver */
#define IS_TAAISP_SIZE		(0)

/* TNR DMA: Moved into driver*/
#define TAAISP_TNR_SIZE		(0)

/* Secure TNR DMA: Moved into driver */
#define TAAISP_TNR_S_SIZE	(0)

/* DDK HEAP: 90MB */
#define IS_HEAP_SIZE		(0x05A00000)

/* Rule checker size for DDK */
#define IS_RCHECKER_SIZE_RO	(SZ_4M + SZ_1M)
#define IS_RCHECKER_SIZE_RW	(SZ_256K)

#define SYSREG_CSIS_BASE_ADDR	(0x15020000)
#define HWFC_INDEX_RESET_ADDR	(0x14AA7050)


enum ext_chain_id {
	ID_ORBMCH_0 = 0,
	ID_ORBMCH_1 = 1,
};

#define INTR_ID_BASE_OFFSET	(INTR_HWIP_MAX)
#define GET_IRQ_ID(y, x)	(x - (INTR_ID_BASE_OFFSET * y))
#define valid_3aaisp_intr_index(intr_index) \
	(intr_index >= 0 && intr_index < INTR_HWIP_MAX)

/* TODO: update below for 9830 */
/* Specific interrupt map belonged to each IP */

/* MC-Scaler */
#define USE_DMA_BUFFER_INDEX		(0) /* 0 ~ 7 */
#define MCSC_OFFSET_ALIGN		(2)
#define MCSC_WIDTH_ALIGN		(2)
#define MCSC_HEIGHT_ALIGN		(2)
#define MCSC_PRECISION			(20)
#define MCSC_POLY_RATIO_UP		(25)
#define MCSC_POLY_QUALITY_RATIO_DOWN	(4)
#define MCSC_POLY_RATIO_DOWN		(16)
#define MCSC_POLY_MAX_RATIO_DOWN	(256)
#define MCSC_POST_RATIO_DOWN		(16)
#define MCSC_POST_MAX_WIDTH		(2048)
/* #define MCSC_POST_WA */
/* #define MCSC_POST_WA_SHIFT	(8)*/	/* 256 = 2^8 */
#define MCSC_USE_DEJAG_TUNING_PARAM	(true)
/* #define MCSC_DNR_USE_TUNING		(true) */	/* DNR and DJAG TUNING PARAM are used exclusively. */
#define MCSC_SETFILE_VERSION		(0x14027435)
#define MCSC_DJAG_ENABLE_SENSOR_BRATIO	(2000)
#define MCSC_LINE_BUF_SIZE		(4880)
#define MCSC_DMA_MIN_WIDTH		(8)
#define HWFC_DMA_ID_OFFSET		(8)
#define ENTRY_HF			ENTRY_M5P	/* Subdev ID of MCSC port for High Frequency */
#define CAC_G2_VERSION			1

#define CSIS0_QCH_EN_ADDR		(0x17030004)
#define CSIS1_QCH_EN_ADDR		(0x17040004)
#define CSIS2_QCH_EN_ADDR		(0x17050004)
#define CSIS3_QCH_EN_ADDR		(0x17060004)
#define CSIS3_1_QCH_EN_ADDR		(0x17090004)

/* LME */
#define LME_IMAGE_MAX_WIDTH		1664
#define LME_IMAGE_MAX_HEIGHT		1248
#define LME_TNR_MODE_MIN_BUFFER_NUM	1

int exynos991_is_dump_clk(struct device *dev);

#define IS_LLC_CACHE_HINT_SHIFT 4

enum is_llc_cache_hint {
	IS_LLC_CACHE_HINT_INVALID = 0,
	IS_LLC_CACHE_HINT_BYPASS_TYPE,
	IS_LLC_CACHE_HINT_CACHE_ALLOC_TYPE,
	IS_LLC_CACHE_HINT_CACHE_NOALLOC_TYPE,
	IS_LLC_CACHE_HINT_VOTF_TYPE,
	IS_LLC_CACHE_HINT_LAST_BUT_SHARED,
	IS_LLC_CACHE_HINT_NOT_USED_FAR,
	IS_LLC_CACHE_HINT_LAST_ACCESS,
	IS_LLC_CACHE_HINT_MAX
};

enum is_llc_sn {
	IS_LLC_SN_DEFAULT = 0,
	IS_LLC_SN_FHD,
	IS_LLC_SN_UHD,
	IS_LLC_SN_8K,
	IS_LLC_SN_PREVIEW,
	IS_LLC_SN_END,
};

struct is_llc_way_num {
	int votf;
	int mcfp;
};

enum pablo_video_probe_num {
	PABLO_VIDEO_PROBE_PAF0S,
	PABLO_VIDEO_PROBE_PAF1S,
	PABLO_VIDEO_PROBE_PAF2S,
	PABLO_VIDEO_PROBE_30S,
	PABLO_VIDEO_PROBE_31S,
	PABLO_VIDEO_PROBE_32S,
	PABLO_VIDEO_PROBE_33S,
	PABLO_VIDEO_PROBE_BYRP,
	PABLO_VIDEO_PROBE_RGBP,
	PABLO_VIDEO_PROBE_MCFP,
	PABLO_VIDEO_PROBE_YUVP,
	PABLO_VIDEO_PROBE_M0S,
	PABLO_VIDEO_PROBE_LME0,
	PABLO_VIDEO_PROBE_MAX
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
#define DEV_HW_ISP_BYRP DEV_HW_BYRP

struct pablo_kunit_subdev_mcs_func {
	int (*mcs_tag_hf)(struct is_device_ischain *device,
				struct is_frame *frame);
};
#endif

#endif
