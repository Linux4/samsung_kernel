/* drivers/video/fbdev/exynos/dpu30/cal_9630/regs-dpp.h
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Register definition file for Samsung dpp driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef DPP_REGS_H_
#define DPP_REGS_H_

/*
 * DPU_DMA SFR base address : 0x14890000
 * - GLOBAL         : 0x14890000
 * - IDMA GF0(L0)   : 0x14891000
 * - IDMA GF1(L1)   : 0x14892000
 * - IDMA VG(L2)    : 0x14893000
 * - IDMA VGS(L3)   : 0x14894000
 */
#define DPU_DMA_VERSION				0x0000
#define DPU_DMA_VER				0x05000000

#define DPU_DMA_QCH_EN				0x000C
#define DMA_QCH_EN				(1 << 0)

#define DPU_DMA_SWRST				0x0010
#define DMA_CH2_SWRST				(1 << 3)
#define DMA_CH1_SWRST				(1 << 2)
#define DMA_CH0_SWRST				(1 << 1)
#define DMA_ALL_SWRST				(1 << 0)
#define DMA_CH_SWRST(_ch)			(1 << ((_ch)))

#define DPU_DMA_TEST_PATTERN0_3			0x0020
#define DPU_DMA_TEST_PATTERN0_2			0x0024
#define DPU_DMA_TEST_PATTERN0_1			0x0028
#define DPU_DMA_TEST_PATTERN0_0			0x002C
#define DPU_DMA_TEST_PATTERN1_3			0x0030
#define DPU_DMA_TEST_PATTERN1_2			0x0034
#define DPU_DMA_TEST_PATTERN1_1			0x0038
#define DPU_DMA_TEST_PATTERN1_0			0x003C

#define DPU_DMA_GLB_CGEN_CH0			0x0040
#define DMA_SFR_CGEN(_v)			((_v) << 31)
#define DMA_SFR_CGEN_MASK			(1 << 31)
#define DMA_INT_CGEN(_v)			((_v) << 0)
#define DMA_INT_CGEN_MASK			(0x7FFFFFFF << 0)

#define DPU_DMA_GLB_CGEN_CH1			0x0044
#define DMA_INT_CGEN(_v)			((_v) << 0)
#define DMA_INT_CGEN_MASK			(0x7FFFFFFF << 0)

#define DPU_DMA_GLB_CGEN_CH2			0x0048
#define DMA_INT_CGEN(_v)			((_v) << 0)
#define DMA_INT_CGEN_MASK			(0x7FFFFFFF << 0)

#define DPU_DMA_DEBUG_CONTROL			0x0100
#define DPU_DMA_DEBUG_CONTROL_SEL(_v)		((_v) << 16)
#define DPU_DMA_DEBUG_CONTROL_EN		(0x1 << 0)

#define DPU_DMA_DEBUG_DATA			0x0104

/*
 * 1.1 - IDMA Register
 * < DMA.offset >
 *  G0      G1      VG      VGS     VGF     VGRFS
 *  0x1000  0x2000  0x3000  0x4000  0x5000  0x6000
 */
#define IDMA_ENABLE				0x0000
#define IDMA_ASSIGNED_MO(_v)			((_v) << 24)
#define IDMA_ASSIGNED_MO_MASK			(0xff << 24)
#define IDMA_SRESET				(1 << 8)
#define IDMA_SFR_UPDATE_FORCE			(1 << 4)
#define IDMA_OP_STATUS				(1 << 2)
#define OP_STATUS_IDLE				(0)
#define OP_STATUS_BUSY				(1)
#define IDMA_INSTANT_OFF_PENDING		(1 << 1)
#define INSTANT_OFF_PENDING			(1)
#define INSTANT_OFF_NOT_PENDING			(0)

#define IDMA_IRQ				0x0004
/* [9630] AXI_ADDR_ERR_IRQ is added */
#define IDMA_AXI_ADDR_ERR_IRQ			(1 << 26)
#define IDMA_AFBC_CONFLICT_IRQ			(1 << 25)
#define IDMA_VR_CONFLICT_IRQ			(1 << 24)
#define IDMA_SBWC_ERR_IRQ			(1 << 23)
#define IDMA_RECOVERY_TRG_IRQ			(1 << 22)
#define IDMA_CONFIG_ERROR			(1 << 21)
#define IDMA_LOCAL_HW_RESET_DONE		(1 << 20)
#define IDMA_READ_SLAVE_ERROR			(1 << 19)
#define IDMA_STATUS_DEADLOCK_IRQ		(1 << 17)
#define IDMA_STATUS_FRAMEDONE_IRQ		(1 << 16)
#define IDMA_ALL_IRQ_CLEAR			(0x7FB << 16)
/* [9630] AXI_ADDR_ERR_IRQ_MASK is added */
#define IDMA_AXI_ADDR_ERR_IRQ_MASK		(1 << 11)
#define IDMA_AFBC_CONFLICT_MASK			(1 << 10)
#define IDMA_VR_CONFLICT_MASK			(1 << 9)
#define IDMA_SBWC_ERR_MASK			(1 << 8)
#define IDMA_RECOVERY_TRG_MASK			(1 << 7)
#define IDMA_CONFIG_ERROR_MASK			(1 << 6)
#define IDMA_LOCAL_HW_RESET_DONE_MASK		(1 << 5)
#define IDMA_READ_SLAVE_ERROR_MASK		(1 << 4)
#define IDMA_IRQ_DEADLOCK_MASK			(1 << 2)
#define IDMA_IRQ_FRAMEDONE_MASK			(1 << 1)
#define IDMA_ALL_IRQ_MASK			(0x7FB << 1)
#define IDMA_IRQ_ENABLE				(1 << 0)

#define IDMA_IN_CON				0x0008
#define IDMA_PIXEL_ALPHA(_v)			((_v) << 24)
#define IDMA_PIXEL_ALPHA_MASK			(0xff << 24)
#define IDMA_IN_IC_MAX(_v)			((_v) << 16)
#define IDMA_IN_IC_MAX_MASK			(0xff << 16)
#define IDMA_SBWC_LOSSY_EN			(1 << 14)
#define IDMA_IMG_FORMAT(_v)			((_v) << 8)
#define IDMA_IMG_FORMAT_MASK			(0x3f << 8)
#define IDMA_IMG_FORMAT_ARGB8888		(0)
#define IDMA_IMG_FORMAT_ABGR8888		(1)
#define IDMA_IMG_FORMAT_RGBA8888		(2)
#define IDMA_IMG_FORMAT_BGRA8888		(3)
#define IDMA_IMG_FORMAT_XRGB8888		(4)
#define IDMA_IMG_FORMAT_XBGR8888		(5)
#define IDMA_IMG_FORMAT_RGBX8888		(6)
#define IDMA_IMG_FORMAT_BGRX8888		(7)
#define IDMA_IMG_FORMAT_RGB565			(8)
#define IDMA_IMG_FORMAT_BGR565			(9)
#define IDMA_IMG_FORMAT_ARGB1555		(12)
#define IDMA_IMG_FORMAT_ARGB4444		(13)
#define IDMA_IMG_FORMAT_ABGR1555		(14)
#define IDMA_IMG_FORMAT_ABGR4444		(15)
#define IDMA_IMG_FORMAT_ARGB2101010		(16)
#define IDMA_IMG_FORMAT_ABGR2101010		(17)
#define IDMA_IMG_FORMAT_RGBA1010102		(18)
#define IDMA_IMG_FORMAT_BGRA1010102		(19)
#define IDMA_IMG_FORMAT_RGBA5551		(20)
#define IDMA_IMG_FORMAT_RGBA4444		(21)
#define IDMA_IMG_FORMAT_BGRA5551		(22)
#define IDMA_IMG_FORMAT_BGRA4444		(23)
#define IDMA_IMG_FORMAT_YUV420_2P		(24)
#define IDMA_IMG_FORMAT_YVU420_2P		(25)
#define IDMA_IMG_FORMAT_YUV420_8P2		(26)
#define IDMA_IMG_FORMAT_YVU420_8P2		(27)
#define IDMA_IMG_FORMAT_YUV420_P010		(29)
#define IDMA_IMG_FORMAT_YVU420_P010		(28)
#define IDMA_IMG_FORMAT_YVU422_2P		(56)
#define IDMA_IMG_FORMAT_YUV422_2P		(57)
#define IDMA_IMG_FORMAT_YVU422_8P2		(58)
#define IDMA_IMG_FORMAT_YUV422_8P2		(59)
#define IDMA_IMG_FORMAT_YVU422_P210		(60)
#define IDMA_IMG_FORMAT_YUV422_P210		(61)
#define IDMA_ROTATION(_v)			((_v) << 4)
#define IDMA_ROTATION_MASK			(0x7 << 4)
#define IDMA_ROTATION_X_FLIP			(1 << 4)
#define IDMA_ROTATION_Y_FLIP			(2 << 4)
#define IDMA_ROTATION_180			(3 << 4)
#define IDMA_ROTATION_90			(4 << 4)
#define IDMA_ROTATION_90_X_FLIP			(5 << 4)
#define IDMA_ROTATION_90_Y_FLIP			(6 << 4)
#define IDMA_ROTATION_270			(7 << 4)
#define IDMA_IN_FLIP(_v)			((_v) << 4)
#define IDMA_IN_FLIP_MASK			(0x3 << 4)
/* #define IDMA_CSET_EN				(1 << 3) */
#define IDMA_SBWC_EN				(1 << 2)
#define IDMA_AFBC_EN				(1 << 1)
#define IDMA_BLOCK_EN				(1 << 0)

#define IDMA_SRC_SIZE				0x0010
#define IDMA_SRC_HEIGHT(_v)			((_v) << 16)
#define IDMA_SRC_HEIGHT_MASK			(0xFFFF << 16)
#define IDMA_SRC_WIDTH(_v)			((_v) << 0)
#define IDMA_SRC_WIDTH_MASK			(0xFFFF << 0)

#define IDMA_SRC_OFFSET				0x0014
#define IDMA_SRC_OFFSET_Y(_v)			((_v) << 16)
#define IDMA_SRC_OFFSET_Y_MASK			(0x3FFF << 16)
#define IDMA_SRC_OFFSET_X(_v)			((_v) << 0)
#define IDMA_SRC_OFFSET_X_MASK			(0x3FFF << 0)

#define IDMA_IMG_SIZE				0x0018
#define IDMA_IMG_HEIGHT(_v)			((_v) << 16)
#define IDMA_IMG_HEIGHT_MASK			(0x3FFF << 16)
#define IDMA_IMG_WIDTH(_v)			((_v) << 0)
#define IDMA_IMG_WIDTH_MASK			(0x3FFF << 0)

#define IDMA_BLOCK_OFFSET			0x0020
#define IDMA_BLK_OFFSET_Y(_v)			((_v) << 16)
#define IDMA_BLK_OFFSET_Y_MASK			(0x3FFF << 16)
#define IDMA_BLK_OFFSET_X(_v)			((_v) << 0)
#define IDMA_BLK_OFFSET_X_MASK			(0x3FFF << 0)

#define IDMA_BLOCK_SIZE				0x0024
#define IDMA_BLK_HEIGHT(_v)			((_v) << 16)
#define IDMA_BLK_HEIGHT_MASK			(0x3FFF << 16)
#define IDMA_BLK_WIDTH(_v)			((_v) << 0)
#define IDMA_BLK_WIDTH_MASK			(0x3FFF << 0)

#define IDMA_IN_BASE_ADDR_Y8			0x0040
#define IDMA_IN_BASE_ADDR_C8			0x0044
#define IDMA_IN_BASE_ADDR_Y2			0x0048
#define IDMA_IN_BASE_ADDR_C2			0x004C

#define IDMA_SRC_STRIDE_0			0x0050
#define IDMA_PLANE_3_STRIDE_SEL			(1 << 23)
#define IDMA_PLANE_2_STRIDE_SEL			(1 << 22)
#define IDMA_PLANE_1_STRIDE_SEL			(1 << 21)
#define IDMA_PLANE_0_STRIDE_SEL			(1 << 20)
#define IDMA_PLANE_STRIDE_SEL(_v)		((_v) << 20)
#define IDMA_PLANE_STRIDE_SEL_MASK		(0xF << 20)
#define IDMA_CHROM_STRIDE_SEL			(1 << 16)
#define IDMA_CHROM_STRIDE(_v)			((_v) << 0)
#define IDMA_CHROM_STRIDE_MASK			(0xFFFF << 0)

#define IDMA_SRC_STRIDE_1			0x0054
#define IDMA_PLANE_1_STRIDE(_v)			((_v) << 16)
#define IDMA_PLANE_1_STRIDE_MASK		(0xffff << 16)
#define IDMA_PLANE_0_STRIDE(_v)			((_v) << 0)
#define IDMA_PLANE_0_STRIDE_MASK		(0xffff << 0)

#define IDMA_SRC_STRIDE_2			0x0058
#define IDMA_PLANE_3_STRIDE(_v)			((_v) << 16)
#define IDMA_PLANE_3_STRIDE_MASK		(0xffff << 16)
#define IDMA_PLANE_2_STRIDE(_v)			((_v) << 0)
#define IDMA_PLANE_2_STRIDE_MASK		(0xffff << 0)

/*
// Not support in 9630
#define IDMA_AFBC_PARAM				0x0060
#define IDMA_AFBC_BLOCK_SIZE(_v)		((_v) << 0)
#define IDMA_AFBC_BLOCK_SIZE_MASK		(0x3 << 0)
#define IDMA_AFBC_BLOCK_SIZE_16_16		(0)
#define IDMA_AFBC_BLOCK_SIZE_32_8		(1)
#define IDMA_AFBC_BLOCK_SIZE_64_4		(2)
*/

/* [9630] SWBC_PARAM is added */
#define IDMA_SBWC_PARAM				0x0064
#define IDMA_SBWC_CRC_EN			(1 << 16)
#define IDMA_CHM_LOSSY_BYTENUM(_v)		((_v) << 8)
#define IDMA_CHM_LOSSY_BYTENUM_MASK		(0xf << 8)
#define IDMA_LUM_LOSSY_BYTENUM(_v)		((_v) << 4)
#define IDMA_LUM_LOSSY_BYTENUM_MASK		(0xf << 4)
#define IDMA_CHM_BLK_SIZE(_v)			((_v) << 2)
#define IDMA_CHM_BLK_SIZE_MASK			(0x3 << 2)
#define IDMA_LUM_BLK_SIZE(_v)			((_v) << 0)
#define IDMA_LUM_BLK_SIZE_MASK			(0x3 << 0)

/*
// Not support in 9630
#define IDMA_CSET_PARAM				0x0068
#define IDMA_GB_BASE(_v)			((_v) << 0)
#define IDMA_GB_BASE_MASK			(0x1f << 0)
*/

#define IDMA_RECOVERY_CTRL			0x0070
#define IDMA_RECOVERY_NUM(_v)			((_v) << 1)
#define IDMA_RECOVERY_NUM_MASK			(0x7fffffff << 1)
#define IDMA_RECOVERY_EN			(1 << 0)

/* [9630] IDMA_DEADLOCK_NUM -> IDMA_DEADLOCK_EN */
#define IDMA_DEADLOCK_EN			0x0100
#define IDMA_DEADLOCK_TIMER(_v)			((_v) << 1)
#define IDMA_DEADLOCK_TIMER_MASK		(0x7fffffff << 1)
#define IDMA_DEADLOCK_TIMER_EN			(1 << 0)

#define IDMA_BUS_CON				0x0110

/* [9630] IDMA_CACHE_CON is added */
#define IDMA_CACHE_CON				0x0114
#define IDMA_DATA_SAHRE_TYPE_P3(_v)		((_v) << 28)
#define IDMA_DATA_SAHRE_TYPE_P3_MASK		(0x3 << 28)
#define IDMA_LLC_HINT_P3(_v)			((_v) << 24)
#define IDMA_LLC_HINT_P3_MASK			(0x7 << 24)
#define IDMA_DATA_SAHRE_TYPE_P2(_v)		((_v) << 20)
#define IDMA_DATA_SAHRE_TYPE_P2_MASK		(0x3 << 20)
#define IDMA_LLC_HINT_P2(_v)			((_v) << 16)
#define IDMA_LLC_HINT_P2_MASK			(0x7 << 16)
#define IDMA_DATA_SAHRE_TYPE_P1(_v)		((_v) << 12)
#define IDMA_DATA_SAHRE_TYPE_P1_MASK		(0x3 << 12)
#define IDMA_LLC_HINT_P1(_v)			((_v) << 8)
#define IDMA_LLC_HINT_P1_MASK			(0x7 << 8)
#define IDMA_DATA_SAHRE_TYPE_P0(_v)		((_v) << 4)
#define IDMA_DATA_SAHRE_TYPE_P0_MASK		(0x3 << 4)
#define IDMA_LLC_HINT_P0(_v)			((_v) << 0)
#define IDMA_LLC_HINT_P0_MASK			(0x7 << 0)

/* [9630] IDMA_PERFORMANCE_CON is added at each layer */
#define IDMA_PERFORMANCE_CON			0x0120
#define IDMA_DEGRADATION_TIME(_v)		((_v) << 16)
#define IDMA_DEGRADATION_TIME_MASK		(0xFFFF << 16)
#define IDMA_IN_IC_MAX_DEG(_v)			((_v) << 4)
#define IDMA_IN_IC_MAX_DEG_MASK			(0xFF << 4)
#define IDMA_DEGRADATION_EN			(1 << 0)

/* [9630] IDMA_QOS_LUT is added at each layer */
/* _n: [0,7], _v: [0x0, 0xF] */
#define IDMA_QOS_LUT07_00			0x0130
#define IDMA_QOS_LUT15_08			0x0134
#define IDMA_QOS_LUT(_n, _v)			((_v) << (4*(_n)))
#define IDMA_QOS_LUT_MASK(_n)			(0xF << (4*(_n)))

#define IDMA_DYNAMIC_GATING_EN			0x0140
#define IDMA_SRAM_CG_EN				(1 << 31)
#define IDMA_DG_EN(_n, _v)			((_v) << (_n))
#define IDMA_DG_EN_MASK(_n)			(1 << (_n))
#define IDMA_DG_EN_ALL				(0x7FFFFFFF << 0)

#define IDMA_DEBUG_CONTROL			0x0300
#define IDMA_DEBUG_CONTROL_SEL(_v)		((_v) << 16)
#define IDMA_DEBUG_CONTROL_EN			(0x1 << 0)

#define IDMA_DEBUG_DATA				0x0304

/* 0: AXI, 3: Pattern */
#define IDMA_IN_REQ_DEST			0x0308
#define IDMA_IN_REG_DEST_SEL(_v)		((_v) << 0)
#define IDMA_IN_REG_DEST_SEL_MASK		(0x3 << 0)

/* [9630] IDMA_PSLV_ERR_CTRL is added */
#define IDMA_PSLV_ERR_CTRL			0x030c
#define IDMA_PSLVERR_CTRL			(1 << 0)

/* [9630] IDMA_DEBUG_ADDR_Y/C is added */
#define IDMA_DEBUG_ADDR_Y8			0x0310
#define IDMA_DEBUG_ADDR_C8			0x0314
#define IDMA_DEBUG_ADDR_Y2			0x0318
#define IDMA_DEBUG_ADDR_C2			0x031C

/* [9630] IDMA_DEBUG_ADDR_CTRL is added */
#define IDMA_DEBUG_ADDR_CTRL			0x0320
#define IDMA_DBG_EN_ADDR_C2			(1 << 3)
#define IDMA_DBG_EN_ADDR_Y2			(1 << 2)
#define IDMA_DBG_EN_ADDR_C8			(1 << 1)
#define IDMA_DBG_EN_ADDR_Y8			(1 << 0)

#define IDMA_CFG_ERR_STATE			0x0b30
#define IDMA_CFG_ERR_ROTATION			(1 << 21)
#define IDMA_CFG_ERR_IMG_HEIGHT_ROTATION	(1 << 20)
#define IDMA_CFG_ERR_AFBC			(1 << 18)
#define IDMA_CFG_ERR_SBWC			(1 << 17)
#define IDMA_CFG_ERR_BLOCK			(1 << 16)
#define IDMA_CFG_ERR_FORMAT			(1 << 15)
#define IDMA_CFG_ERR_STRIDE3			(1 << 14)
#define IDMA_CFG_ERR_STRIDE2			(1 << 13)
#define IDMA_CFG_ERR_STRIDE1			(1 << 12)
#define IDMA_CFG_ERR_STRIDE0			(1 << 11)
#define IDMA_CFG_ERR_CHROM_STRIDE		(1 << 10)
#define IDMA_CFG_ERR_BASE_ADDR_C2		(1 << 9)
#define IDMA_CFG_ERR_BASE_ADDR_Y2		(1 << 8)
#define IDMA_CFG_ERR_BASE_ADDR_C8		(1 << 7)
#define IDMA_CFG_ERR_BASE_ADDR_Y8		(1 << 6)
#define IDMA_CFG_ERR_SRC_OFFSET_Y		(1 << 5)
#define IDMA_CFG_ERR_SRC_OFFSET_X		(1 << 4)
#define IDMA_CFG_ERR_IMG_HEIGHT			(1 << 3)
#define IDMA_CFG_ERR_IMG_WIDTH			(1 << 2)
#define IDMA_CFG_ERR_SRC_HEIGHT			(1 << 1)
#define IDMA_CFG_ERR_SRC_WIDTH			(1 << 0)
#define IDMA_CFG_ERR_GET(_v)			(((_v) >> 0) & 0x3FFFFF)

/*
 * ODMA SFR list
 * base address : 
 */
/* EMPTY */


#define DMA_SHD_OFFSET				0x800
// ADD field def



/*
 * 2 - DPU_WB_MUX.base
 *  Non-secure :
 */
/* EMPTY */

/*
 * DPP SFR base address : 0x14841000
 * - DPP GF0(L0)   : 0x14841000
 * - DPP GF1(L1)   : 0x14842000
 * - DPP VG(L2)    : 0x14843000
 * - DPP VGF(L3)   : 0x14844000
 */
#define DPP_ENABLE				0x0000
#define DPP_SRSET				(1 << 24)
#define DPP_HDR_SEL				(0 << 11)
#define DPP_SFR_CLOCK_GATE_EN			(1 << 10)
#define DPP_SRAM_CLOCK_GATE_EN			(1 << 9)
#define DPP_INT_CLOCK_GATE_EN			(1 << 8)
#define DPP_ALL_CLOCK_GATE_EN_MASK		(0x7 << 8)
#define DPP_PSLVERR_EN				(1 << 5)
#define DPP_SFR_UPDATE_FORCE			(1 << 4)
#define DPP_QCHANNEL_EN				(1 << 3)
#define DPP_OP_STATUS				(1 << 2)
#define DPP_TZPC_FLAG				(1 << 0)

#define DPP_IRQ					0x0004
#define DPP_CONFIG_ERROR			(1 << 21)
#define DPP_FRAMEDONE_IRQ			(1 << 16)
#define DPP_ALL_IRQ_CLEAR			(0x21 << 16)
#define DPP_CONFIG_ERROR_MASK			(1 << 6)
#define DPP_FRAMEDONE_IRQ_MASK			(1 << 1)
#define DPP_ALL_IRQ_MASK			(0x21 << 1)
#define DPP_IRQ_ENABLE				(1 << 0)

#define DPP_IN_CON				0x0008
#define DPP_CSC_TYPE(_v)			((_v) << 18)
#define DPP_CSC_TYPE_MASK			(3 << 18)
#define DPP_CSC_RANGE(_v)			((_v) << 17)
#define DPP_CSC_RANGE_MASK			(1 << 17)
#define DPP_CSC_MODE(_v)			((_v) << 16)
#define DPP_CSC_MODE_MASK			(1 << 16)
#define DPP_DEPREMULTIPLY_EN			(1 << 7)
#define DPP_PREMULTIPLY_EN			(1 << 6)
#define DPP_DITH_MASK_SEL			(1 << 5)
#define DPP_DITH_MASK_SPIN			(1 << 4)
#define DPP_ALPHA_SEL(_v)			((_v) << 3)
#define DPP_ALPHA_SEL_MASK			(1 << 3)
#define DPP_IMG_FORMAT(_v)			((_v) << 0)
#define DPP_IMG_FORMAT_MASK			(0x7 << 0)
#define DPP_IMG_FORMAT_ARGB8888			(0 << 0)
#define DPP_IMG_FORMAT_ARGB8101010		(1 << 0)
#define DPP_IMG_FORMAT_YUV420_8P		(2 << 0)
#define DPP_IMG_FORMAT_YUV420_P010		(3 << 0)
#define DPP_IMG_FORMAT_YUV420_8P2		(4 << 0)
#define DPP_IMG_FORMAT_YUV422_8P		(5 << 0)
#define DPP_IMG_FORMAT_YUV422_P210		(6 << 0)
#define DPP_IMG_FORMAT_YUV422_8P2		(7 << 0)

#define DPP_IMG_SIZE				0x0018
#define DPP_IMG_HEIGHT(_v)			((_v) << 16)
#define DPP_IMG_HEIGHT_MASK			(0x1FFF << 16)
#define DPP_IMG_WIDTH(_v)			((_v) << 0)
#define DPP_IMG_WIDTH_MASK			(0x1FFF << 0)

/* scaler configuration only */
#define DPP_SCALED_IMG_SIZE			0x002C
#define DPP_SCALED_IMG_HEIGHT(_v)		((_v) << 16)
#define DPP_SCALED_IMG_HEIGHT_MASK		(0x1FFF << 16)
#define DPP_SCALED_IMG_WIDTH(_v)		((_v) << 0)
#define DPP_SCALED_IMG_WIDTH_MASK		(0x1FFF << 0)

/*
 * (00-01-02) : Reg0.L-Reg0.H-Reg1.L
 * (10-11-12) : Reg1.H-Reg2.L-Reg2.H
 * (20-21-22) : Reg3.L-Reg3.H-Reg4.L
 */
#define DPP_CSC_COEF0				0x0030
#define DPP_CSC_COEF1				0x0034
#define DPP_CSC_COEF2				0x0038
#define DPP_CSC_COEF3				0x003C
#define DPP_CSC_COEF4				0x0040
#define DPP_CSC_COEF_H(_v)			((_v) << 16)
#define DPP_CSC_COEF_H_MASK			(0xFFFF << 16)
#define DPP_CSC_COEF_L(_v)			((_v) << 0)
#define DPP_CSC_COEF_L_MASK			(0xFFFF << 0)
#define DPP_CSC_COEF_XX(_n, _v)			((_v) << (0 + (16 * (_n))))
#define DPP_CSC_COEF_XX_MASK(_n)		(0xFFF << (0 + (16 * (_n))))

#define DPP_MAIN_H_RATIO			0x0044
#define DPP_H_RATIO(_v)				((_v) << 0)
#define DPP_H_RATIO_MASK			(0xFFFFFF << 0)

#define DPP_MAIN_V_RATIO			0x0048
#define DPP_V_RATIO(_v)				((_v) << 0)
#define DPP_V_RATIO_MASK			(0xFFFFFF << 0)

#define DPP_Y_VCOEF_0A				0x0200
#define DPP_Y_HCOEF_0A				0x0290
#define DPP_C_VCOEF_0A				0x0400
#define DPP_C_HCOEF_0A				0x0490
#define DPP_SCL_COEF(_v)			((_v) << 0)
#define DPP_SCL_COEF_MASK			(0x7FF << 0)
#define DPP_H_COEF(n, s, x)			(0x290 + (n) * 0x4 + (s) * 0x24 + (x) * 0x200)
#define DPP_V_COEF(n, s, x)			(0x200 + (n) * 0x4 + (s) * 0x24 + (x) * 0x200)

#define DPP_YHPOSITION				0x05B0
#define DPP_YVPOSITION				0x05B4
#define DPP_CHPOSITION				0x05B8
#define DPP_CVPOSITION				0x05BC
#define DPP_POS_I(_v)				((_v) << 20)
#define DPP_POS_I_MASK				(0xFFF << 20)
#define DPP_POS_I_GET(_v)			(((_v) >> 20) & 0xFFF)
#define DPP_POS_F(_v)				((_v) << 0)
#define DPP_POS_F_MASK				(0xFFFFF << 0)
#define DPP_POS_F_GET(_v)			(((_v) >> 0) & 0xFFFFF)

/* 0x0A00 ~ 0x0A1C : ASHE */

#define DPP_DYNAMIC_GATING_EN			0x0A54
/* _n: [0 ~ 4, 6], v: [0,1] */
#define DPP_DG_EN(_n, _v)			((_v) << (_n))
#define DPP_DG_EN_MASK(_n)			(1 << (_n))
#define DPP_DG_EN_ALL				(0x5F << 0)

#define DPP_LINECNT_CON				0x0D00
#define DPP_LC_CAPTURE(_v)			((_v) << 2)
#define DPP_LC_CAPTURE_MASK			(1 << 2)
#define DPP_LC_MODE(_V)				((_V) << 1)
#define DPP_LC_MODE_MASK			(1 << 1)
#define DPP_LC_ENABLE(_v)			((_v) << 0)
#define DPP_LC_ENABLE_MASK			(1 << 0)

#define DPP_LINECNT_VAL				0x0D04
#define DPP_LC_COUNTER(_v)			((_v) << 0)
#define DPP_LC_COUNTER_MASK			(0x1FFF << 0)
#define DPP_LC_COUNTER_GET(_v)			(((_v) >> 0) & 0x1FFF)

#define DPP_CFG_ERR_STATE			0x0D08
#define DPP_CFG_ERR_SCL_POS			(1 << 4)
#define DPP_CFG_ERR_SCALE_RATIO			(1 << 3)
#define DPP_CFG_ERR_ODD_SIZE			(1 << 2)
#define DPP_CFG_ERR_MAX_SIZE			(1 << 1)
#define DPP_CFG_ERR_MIN_SIZE			(1 << 0)
#define DPP_CFG_ERR_GET(_v)			(((_v) >> 0) & 0x1F)

/*
 * 9630 : no HDR Layer
 * E9630 doesn't support HDR, but we've left it because of the compilation.
 */

/* HDR section */
#define HDR_LSI_LN_VERSION 		 	0x0000
#define HDR_LSI_LN_COM_CTRL 		 	0x0004
#define HDR_LSI_LN_COM_CTRL_MODE(_v) 		((_v) << 31)
#define HDR_LSI_LN_COM_CTRL_MODE_MASK 		(1 << 31)
#define HDR_LSI_LN_COM_CTRL_EN 		 	(1 << 0)
#define HDR_LSI_LN_COM_CTRL_EN_MASK	 	(1 << 0)
#define HDR_LSI_LN_MOD_CTRL 		 	0x0008
#define HDR_LSI_LN_MOD_CTRL_TEN 		(1 << 5)
#define HDR_LSI_LN_MOD_CTRL_AEN 		(1 << 4)
#define HDR_LSI_LN_MOD_CTRL_SEN 		(1 << 3)
#define HDR_LSI_LN_MOD_CTRL_GEN 		(1 << 2)
#define HDR_LSI_LN_MOD_CTRL_EEN 		(1 << 1)
#define HDR_LSI_LN_MOD_CTRL_OEN 		(1 << 0)

#define HDR_LSI_LN_OETF_POSX(_n) 		(((_n) / 2) * (0x4) + (0x000c))
#define HDR_LSI_LN_OETF_POSX_VAL(_n, _v) \
		(((_n) % (2)) ? (((_v) & 0xFFFF) << 16) : (((_v) & 0xFFFF) << 0))
#define HDR_LSI_LN_OETF_POSX_MASK(_n) \
		(((_n) % 2) ? (0xFFFF << 16) : (0xFFFF << 0))

#define HDR_LSI_LN_OETF_POSY(_n) 		(((_n) / 2) * (0x4) + (0x0050))
#define HDR_LSI_LN_OETF_POSY_VAL(_n, _v) \
		(((_n) % (2)) ? (((_v) & 0xFFF) << 16) : (((_v) & 0xFFF) << 0))
#define HDR_LSI_LN_OETF_POSY_MASK(_n) \
		(((_n) % 2) ? (0xFFF << 16) : (0xFFF << 0))

#define HDR_LSI_LN_EOTF_POSX(_n) 		(((_n) / 2) * (0x4) + (0x0094))
#define HDR_LSI_LN_EOTF_POSX_VAL(_n, _v) \
		(((_n) % (2)) ? (((_v) & 0x3FF) << 16) : (((_v) & 0x3FF) << 0))
#define HDR_LSI_LN_EOTF_POSX_MASK(_n) \
		(((_n) % 2) ? (0x3FF << 16) : (0x3FF << 0))

#define HDR_LSI_LN_EOTF_POSY(_n) 		((0x0198) + ((0x4) * (_n)))
#define HDR_LSI_LN_EOTF_POSY_VAL(_v) 		(((_v) & 0x3FFFF) << 0)
#define HDR_LSI_LN_EOTF_POSY_MASK(_n) 		(0x3FFFF << 0)

#define HDR_LSI_LN_GM_COEF(_n) 		 	((0x039c) + ((0x4) * (_n)))
#define HDR_LSI_LN_GM_COEF_VAL(_v) 		(((_v) & 0x7FFFF) << 0)
#define HDR_LSI_LN_TS_GAIN 		 	0x03c0
#define HDR_LSI_LN_TS_GAIN_VAL(_v) 		(((_v) & 0xFFF) << 0)

#define HDR_LSI_LN_AM_CTRL 		 	0x03c4
#define HDR_LSI_LN_AM_CTRL_KNEE_VAL(_v) 	(((_v) & 0xFFF) << 9)
#define HDR_LSI_LN_AM_CTRL_WGHT_VAL(_v) 	(((_v) & 0x1FF) << 0)
#define HDR_LSI_LN_AM_POSX(_n) 			(((_n) / 2) * (0x4) + (0x03c8))
#define HDR_LSI_LN_AM_POSX_VAL(_n, _v) \
		(((_n) % (2)) ? (((_v) & 0xFFF) << 16) : (((_v) & 0xFFF) << 0))
#define HDR_LSI_LN_AM_POSY(_n) 			((0x03ec) + ((0x4) * (_n)))
#define HDR_LSI_LN_AM_POSY_VAL(_v) 		(((_v) & 0x1FFFF) << 0)

#define HDR_LSI_LN_TM_CTRL 			0x0430
#define HDR_LSI_LN_TM_CTRL_KNEE_VAL(_v) 	(((_v) & 0xFFFF) << 4)
#define HDR_LSI_LN_TM_CTRL_WGHT_VAL(_v) 	(((_v) & 0xF) << 0)

#define HDR_LSI_LN_TM_COEF 			0x0434
#define HDR_LSI_LN_TM_COEFB_VAL(_v) 		(((_v) & 0x3FF) << 20)
#define HDR_LSI_LN_TM_COEFG_VAL(_v) 		(((_v) & 0x3FF) << 10)
#define HDR_LSI_LN_TM_COEFR_VAL(_v) 		(((_v) & 0x3FF) << 0)

#define HDR_LSI_LN_TM_RNGX 			0x0438
#define HDR_LSI_LN_TM_RNGX_MAXY_VAL(_v) 	(((_v) & 0xFFFF) << 16)
#define HDR_LSI_LN_TM_RNGX_MINY_VAL(_v) 	(((_v) & 0xFFFF) << 0)
#define HDR_LSI_LN_TM_RNGY 			0x043c
#define HDR_LSI_LN_TM_RNGY_MAXY_VAL(_v) 	(((_v) & 0x1FF) << 9)
#define HDR_LSI_LN_TM_RNGY_MINY_VAL(_v) 	(((_v) & 0x1FF) << 0)
#define HDR_LSI_LN_TM_MAXI 			0x0440
#define HDR_LSI_LN_TM_MAXI_VAL(_v) 		(((_v) & 0x3FFFF) << 0)
#define HDR_LSI_LN_TM_MAXP 			0x0444
#define HDR_LSI_LN_TM_MAXP_VAL(_v) 		(((_v) & 0xFFFF) << 0)
#define HDR_LSI_LN_TM_MAXO 			0x0448
#define HDR_LSI_LN_TM_MAXO_VAL(_v) 		(((_v) & 0x3FFFF) << 0)
#define HDR_LSI_LN_TM_POSX(_n) 			(((_n) / 2) * (0x4) + (0x044c))
#define HDR_LSI_LN_TM_POSX_VAL(_n, _v) \
		(((_n) % (2)) ? (((_v) & 0xFFFF) << 16) : (((_v) & 0xFFFF) << 0))
#define HDR_LSI_LN_TM_POSX_MASK(_n) \
		(((_n) % 2) ? (0xFFFF << 16) : (0xFFFF << 0))

#define HDR_LSI_LN_TM_POSY(_n) 		 	((0x0490) + ((0x4) * (_n)))
#define HDR_LSI_LN_TM_POSY_VAL(_v) 		(((_v) & 0x7FFFFFF) << 0)
#define HDR_LSI_LN_TM_POSY_MASK(_n)		((0x7FFFFFF) << 0)

/* shadow register */
#define HDR_LSI_LN_S_COM_CTRL			0x0804
#define HDR_LSI_LN_S_MOD_CTRL			0x0808
#define HDR_LSI_LN_S_OETF_POSX(_n)		((0x080c) + ((0x4) * (_n)))
#define HDR_LSI_LN_S_OETF_POSY(_n)		((0x0850) + ((0x4) * (_n)))
#define HDR_LSI_LN_S_EOTF_POSX(_n)		((0x0894) + ((0x4) * (_n)))
#define HDR_LSI_LN_S_EOTF_POSY(_n)		((0x0998) + ((0x4) * (_n)))
#define HDR_LSI_LN_S_GM_COEF(_n)		((0x0b9c) + ((0x4) * (_n)))
#define HDR_LSI_LN_S_TS_GAIN			0x0bc0
#define HDR_LSI_LN_S_AM_WGHT			0x0bc4
#define HDR_LSI_LN_S_AM_POSX(_n)		((0x0bc8) + ((0x4) * (_n)))
#define HDR_LSI_LN_S_AM_POSY(_n)		((0x0bec) + ((0x4) * (_n)))
#define HDR_LSI_LN_S_TM_CTRL			0x0c30
#define HDR_LSI_LN_S_TM_COEF			0x0c34
#define HDR_LSI_LN_S_TM_RNGX			0x0c38
#define HDR_LSI_LN_S_TM_RNGY			0x0c3c
#define HDR_LSI_LN_S_TM_MAXI			0x0c40
#define HDR_LSI_LN_S_TM_MAXP			0x0c44
#define HDR_LSI_LN_S_TM_MAXO			0x0c48
#define HDR_LSI_LN_S_TM_POSX(_n)		((0x0c4c) + ((0x4) * (_n)))
#define HDR_LSI_LN_S_TM_POSY(_n)		((0x0c90) + ((0x4) * (_n)))

/* Not applied macro SFR for LUT, this is for reference */
#define HDR_LSI_L0_OETF_POSX0			0xc00c
#define HDR_LSI_L0_OETF_POSX1			0xc010
#define HDR_LSI_L0_OETF_POSX2			0xc014
#define HDR_LSI_L0_OETF_POSX3			0xc018
#define HDR_LSI_L0_OETF_POSX4			0xc01c
#define HDR_LSI_L0_OETF_POSX5			0xc020
#define HDR_LSI_L0_OETF_POSX6			0xc024
#define HDR_LSI_L0_OETF_POSX7			0xc028
#define HDR_LSI_L0_OETF_POSX8			0xc02c
#define HDR_LSI_L0_OETF_POSX9			0xc030
#define HDR_LSI_L0_OETF_POSX10			0xc034
#define HDR_LSI_L0_OETF_POSX11			0xc038
#define HDR_LSI_L0_OETF_POSX12			0xc03c
#define HDR_LSI_L0_OETF_POSX13			0xc040
#define HDR_LSI_L0_OETF_POSX14			0xc044
#define HDR_LSI_L0_OETF_POSX15			0xc048
#define HDR_LSI_L0_OETF_POSX16			0xc04c
#define HDR_LSI_L0_OETF_POSY0			0xc050
#define HDR_LSI_L0_OETF_POSY1			0xc054
#define HDR_LSI_L0_OETF_POSY2			0xc058
#define HDR_LSI_L0_OETF_POSY3			0xc05c
#define HDR_LSI_L0_OETF_POSY4			0xc060
#define HDR_LSI_L0_OETF_POSY5			0xc064
#define HDR_LSI_L0_OETF_POSY6			0xc068
#define HDR_LSI_L0_OETF_POSY7			0xc06c
#define HDR_LSI_L0_OETF_POSY8			0xc070
#define HDR_LSI_L0_OETF_POSY9			0xc074
#define HDR_LSI_L0_OETF_POSY10			0xc078
#define HDR_LSI_L0_OETF_POSY11			0xc07c
#define HDR_LSI_L0_OETF_POSY12			0xc080
#define HDR_LSI_L0_OETF_POSY13			0xc084
#define HDR_LSI_L0_OETF_POSY14			0xc088
#define HDR_LSI_L0_OETF_POSY15			0xc08c
#define HDR_LSI_L0_OETF_POSY16			0xc090
#define HDR_LSI_L0_EOTF_POSX0			0xc094
#define HDR_LSI_L0_EOTF_POSX1			0xc098
#define HDR_LSI_L0_EOTF_POSX2			0xc09c
#define HDR_LSI_L0_EOTF_POSX3			0xc0a0
#define HDR_LSI_L0_EOTF_POSX4			0xc0a4
#define HDR_LSI_L0_EOTF_POSX5			0xc0a8
#define HDR_LSI_L0_EOTF_POSX6			0xc0ac
#define HDR_LSI_L0_EOTF_POSX7			0xc0b0
#define HDR_LSI_L0_EOTF_POSX8			0xc0b4
#define HDR_LSI_L0_EOTF_POSX9			0xc0b8
#define HDR_LSI_L0_EOTF_POSX10			0xc0bc
#define HDR_LSI_L0_EOTF_POSX11			0xc0c0
#define HDR_LSI_L0_EOTF_POSX12			0xc0c4
#define HDR_LSI_L0_EOTF_POSX13			0xc0c8
#define HDR_LSI_L0_EOTF_POSX14			0xc0cc
#define HDR_LSI_L0_EOTF_POSX15			0xc0d0
#define HDR_LSI_L0_EOTF_POSX16			0xc0d4
#define HDR_LSI_L0_EOTF_POSX17			0xc0d8
#define HDR_LSI_L0_EOTF_POSX18			0xc0dc
#define HDR_LSI_L0_EOTF_POSX19			0xc0e0
#define HDR_LSI_L0_EOTF_POSX20			0xc0e4
#define HDR_LSI_L0_EOTF_POSX21			0xc0e8
#define HDR_LSI_L0_EOTF_POSX22			0xc0ec
#define HDR_LSI_L0_EOTF_POSX23			0xc0f0
#define HDR_LSI_L0_EOTF_POSX24			0xc0f4
#define HDR_LSI_L0_EOTF_POSX25			0xc0f8
#define HDR_LSI_L0_EOTF_POSX26			0xc0fc
#define HDR_LSI_L0_EOTF_POSX27			0xc100
#define HDR_LSI_L0_EOTF_POSX28			0xc104
#define HDR_LSI_L0_EOTF_POSX29			0xc108
#define HDR_LSI_L0_EOTF_POSX30			0xc10c
#define HDR_LSI_L0_EOTF_POSX31			0xc110
#define HDR_LSI_L0_EOTF_POSX32			0xc114
#define HDR_LSI_L0_EOTF_POSX33			0xc118
#define HDR_LSI_L0_EOTF_POSX34			0xc11c
#define HDR_LSI_L0_EOTF_POSX35			0xc120
#define HDR_LSI_L0_EOTF_POSX36			0xc124
#define HDR_LSI_L0_EOTF_POSX37			0xc128
#define HDR_LSI_L0_EOTF_POSX38			0xc12c
#define HDR_LSI_L0_EOTF_POSX39			0xc130
#define HDR_LSI_L0_EOTF_POSX40			0xc134
#define HDR_LSI_L0_EOTF_POSX41			0xc138
#define HDR_LSI_L0_EOTF_POSX42			0xc13c
#define HDR_LSI_L0_EOTF_POSX43			0xc140
#define HDR_LSI_L0_EOTF_POSX44			0xc144
#define HDR_LSI_L0_EOTF_POSX45			0xc148
#define HDR_LSI_L0_EOTF_POSX46			0xc14c
#define HDR_LSI_L0_EOTF_POSX47			0xc150
#define HDR_LSI_L0_EOTF_POSX48			0xc154
#define HDR_LSI_L0_EOTF_POSX49			0xc158
#define HDR_LSI_L0_EOTF_POSX50			0xc15c
#define HDR_LSI_L0_EOTF_POSX51			0xc160
#define HDR_LSI_L0_EOTF_POSX52			0xc164
#define HDR_LSI_L0_EOTF_POSX53			0xc168
#define HDR_LSI_L0_EOTF_POSX54			0xc16c
#define HDR_LSI_L0_EOTF_POSX55			0xc170
#define HDR_LSI_L0_EOTF_POSX56			0xc174
#define HDR_LSI_L0_EOTF_POSX57			0xc178
#define HDR_LSI_L0_EOTF_POSX58			0xc17c
#define HDR_LSI_L0_EOTF_POSX59			0xc180
#define HDR_LSI_L0_EOTF_POSX60			0xc184
#define HDR_LSI_L0_EOTF_POSX61			0xc188
#define HDR_LSI_L0_EOTF_POSX62			0xc18c
#define HDR_LSI_L0_EOTF_POSX63			0xc190
#define HDR_LSI_L0_EOTF_POSX64			0xc194
#define HDR_LSI_L0_EOTF_POSY0			0xc198
#define HDR_LSI_L0_EOTF_POSY1			0xc19c
#define HDR_LSI_L0_EOTF_POSY2			0xc1a0
#define HDR_LSI_L0_EOTF_POSY3			0xc1a4
#define HDR_LSI_L0_EOTF_POSY4			0xc1a8
#define HDR_LSI_L0_EOTF_POSY5			0xc1ac
#define HDR_LSI_L0_EOTF_POSY6			0xc1b0
#define HDR_LSI_L0_EOTF_POSY7			0xc1b4
#define HDR_LSI_L0_EOTF_POSY8			0xc1b8
#define HDR_LSI_L0_EOTF_POSY9			0xc1bc
#define HDR_LSI_L0_EOTF_POSY10			0xc1c0
#define HDR_LSI_L0_EOTF_POSY11			0xc1c4
#define HDR_LSI_L0_EOTF_POSY12			0xc1c8
#define HDR_LSI_L0_EOTF_POSY13			0xc1cc
#define HDR_LSI_L0_EOTF_POSY14			0xc1d0
#define HDR_LSI_L0_EOTF_POSY15			0xc1d4
#define HDR_LSI_L0_EOTF_POSY16			0xc1d8
#define HDR_LSI_L0_EOTF_POSY17			0xc1dc
#define HDR_LSI_L0_EOTF_POSY18			0xc1e0
#define HDR_LSI_L0_EOTF_POSY19			0xc1e4
#define HDR_LSI_L0_EOTF_POSY20			0xc1e8
#define HDR_LSI_L0_EOTF_POSY21			0xc1ec
#define HDR_LSI_L0_EOTF_POSY22			0xc1f0
#define HDR_LSI_L0_EOTF_POSY23			0xc1f4
#define HDR_LSI_L0_EOTF_POSY24			0xc1f8
#define HDR_LSI_L0_EOTF_POSY25			0xc1fc
#define HDR_LSI_L0_EOTF_POSY26			0xc200
#define HDR_LSI_L0_EOTF_POSY27			0xc204
#define HDR_LSI_L0_EOTF_POSY28			0xc208
#define HDR_LSI_L0_EOTF_POSY29			0xc20c
#define HDR_LSI_L0_EOTF_POSY30			0xc210
#define HDR_LSI_L0_EOTF_POSY31			0xc214
#define HDR_LSI_L0_EOTF_POSY32			0xc218
#define HDR_LSI_L0_EOTF_POSY33			0xc21c
#define HDR_LSI_L0_EOTF_POSY34			0xc220
#define HDR_LSI_L0_EOTF_POSY35			0xc224
#define HDR_LSI_L0_EOTF_POSY36			0xc228
#define HDR_LSI_L0_EOTF_POSY37			0xc22c
#define HDR_LSI_L0_EOTF_POSY38			0xc230
#define HDR_LSI_L0_EOTF_POSY39			0xc234
#define HDR_LSI_L0_EOTF_POSY40			0xc238
#define HDR_LSI_L0_EOTF_POSY41			0xc23c
#define HDR_LSI_L0_EOTF_POSY42			0xc240
#define HDR_LSI_L0_EOTF_POSY43			0xc244
#define HDR_LSI_L0_EOTF_POSY44			0xc248
#define HDR_LSI_L0_EOTF_POSY45			0xc24c
#define HDR_LSI_L0_EOTF_POSY46			0xc250
#define HDR_LSI_L0_EOTF_POSY47			0xc254
#define HDR_LSI_L0_EOTF_POSY48			0xc258
#define HDR_LSI_L0_EOTF_POSY49			0xc25c
#define HDR_LSI_L0_EOTF_POSY50			0xc260
#define HDR_LSI_L0_EOTF_POSY51			0xc264
#define HDR_LSI_L0_EOTF_POSY52			0xc268
#define HDR_LSI_L0_EOTF_POSY53			0xc26c
#define HDR_LSI_L0_EOTF_POSY54			0xc270
#define HDR_LSI_L0_EOTF_POSY55			0xc274
#define HDR_LSI_L0_EOTF_POSY56			0xc278
#define HDR_LSI_L0_EOTF_POSY57			0xc27c
#define HDR_LSI_L0_EOTF_POSY58			0xc280
#define HDR_LSI_L0_EOTF_POSY59			0xc284
#define HDR_LSI_L0_EOTF_POSY60			0xc288
#define HDR_LSI_L0_EOTF_POSY61			0xc28c
#define HDR_LSI_L0_EOTF_POSY62			0xc290
#define HDR_LSI_L0_EOTF_POSY63			0xc294
#define HDR_LSI_L0_EOTF_POSY64			0xc298
#define HDR_LSI_L0_EOTF_POSY65			0xc29c
#define HDR_LSI_L0_EOTF_POSY66			0xc2a0
#define HDR_LSI_L0_EOTF_POSY67			0xc2a4
#define HDR_LSI_L0_EOTF_POSY68			0xc2a8
#define HDR_LSI_L0_EOTF_POSY69			0xc2ac
#define HDR_LSI_L0_EOTF_POSY70			0xc2b0
#define HDR_LSI_L0_EOTF_POSY71			0xc2b4
#define HDR_LSI_L0_EOTF_POSY72			0xc2b8
#define HDR_LSI_L0_EOTF_POSY73			0xc2bc
#define HDR_LSI_L0_EOTF_POSY74			0xc2c0
#define HDR_LSI_L0_EOTF_POSY75			0xc2c4
#define HDR_LSI_L0_EOTF_POSY76			0xc2c8
#define HDR_LSI_L0_EOTF_POSY77			0xc2cc
#define HDR_LSI_L0_EOTF_POSY78			0xc2d0
#define HDR_LSI_L0_EOTF_POSY79			0xc2d4
#define HDR_LSI_L0_EOTF_POSY80			0xc2d8
#define HDR_LSI_L0_EOTF_POSY81			0xc2dc
#define HDR_LSI_L0_EOTF_POSY82			0xc2e0
#define HDR_LSI_L0_EOTF_POSY83			0xc2e4
#define HDR_LSI_L0_EOTF_POSY84			0xc2e8
#define HDR_LSI_L0_EOTF_POSY85			0xc2ec
#define HDR_LSI_L0_EOTF_POSY86			0xc2f0
#define HDR_LSI_L0_EOTF_POSY87			0xc2f4
#define HDR_LSI_L0_EOTF_POSY88			0xc2f8
#define HDR_LSI_L0_EOTF_POSY89			0xc2fc
#define HDR_LSI_L0_EOTF_POSY90			0xc300
#define HDR_LSI_L0_EOTF_POSY91			0xc304
#define HDR_LSI_L0_EOTF_POSY92			0xc308
#define HDR_LSI_L0_EOTF_POSY93			0xc30c
#define HDR_LSI_L0_EOTF_POSY94			0xc310
#define HDR_LSI_L0_EOTF_POSY95			0xc314
#define HDR_LSI_L0_EOTF_POSY96			0xc318
#define HDR_LSI_L0_EOTF_POSY97			0xc31c
#define HDR_LSI_L0_EOTF_POSY98			0xc320
#define HDR_LSI_L0_EOTF_POSY99			0xc324
#define HDR_LSI_L0_EOTF_POSY100			0xc328
#define HDR_LSI_L0_EOTF_POSY101			0xc32c
#define HDR_LSI_L0_EOTF_POSY102			0xc330
#define HDR_LSI_L0_EOTF_POSY103			0xc334
#define HDR_LSI_L0_EOTF_POSY104			0xc338
#define HDR_LSI_L0_EOTF_POSY105			0xc33c
#define HDR_LSI_L0_EOTF_POSY106			0xc340
#define HDR_LSI_L0_EOTF_POSY107			0xc344
#define HDR_LSI_L0_EOTF_POSY108			0xc348
#define HDR_LSI_L0_EOTF_POSY109			0xc34c
#define HDR_LSI_L0_EOTF_POSY110			0xc350
#define HDR_LSI_L0_EOTF_POSY111			0xc354
#define HDR_LSI_L0_EOTF_POSY112			0xc358
#define HDR_LSI_L0_EOTF_POSY113			0xc35c
#define HDR_LSI_L0_EOTF_POSY114			0xc360
#define HDR_LSI_L0_EOTF_POSY115			0xc364
#define HDR_LSI_L0_EOTF_POSY116			0xc368
#define HDR_LSI_L0_EOTF_POSY117			0xc36c
#define HDR_LSI_L0_EOTF_POSY118			0xc370
#define HDR_LSI_L0_EOTF_POSY119			0xc374
#define HDR_LSI_L0_EOTF_POSY120			0xc378
#define HDR_LSI_L0_EOTF_POSY121			0xc37c
#define HDR_LSI_L0_EOTF_POSY122			0xc380
#define HDR_LSI_L0_EOTF_POSY123			0xc384
#define HDR_LSI_L0_EOTF_POSY124			0xc388
#define HDR_LSI_L0_EOTF_POSY125			0xc38c
#define HDR_LSI_L0_EOTF_POSY126			0xc390
#define HDR_LSI_L0_EOTF_POSY127			0xc394
#define HDR_LSI_L0_EOTF_POSY128			0xc398
#define HDR_LSI_L0_GM_COEF0			0xc39c
#define HDR_LSI_L0_GM_COEF1			0xc3a0
#define HDR_LSI_L0_GM_COEF2			0xc3a4
#define HDR_LSI_L0_GM_COEF3			0xc3a8
#define HDR_LSI_L0_GM_COEF4			0xc3ac
#define HDR_LSI_L0_GM_COEF5			0xc3b0
#define HDR_LSI_L0_GM_COEF6			0xc3b4
#define HDR_LSI_L0_GM_COEF7			0xc3b8
#define HDR_LSI_L0_GM_COEF8			0xc3bc
#define HDR_LSI_L0_S_OETF_POSX0			0xc80c
#define HDR_LSI_L0_S_OETF_POSX1			0xc810
#define HDR_LSI_L0_S_OETF_POSX2			0xc814
#define HDR_LSI_L0_S_OETF_POSX3			0xc818
#define HDR_LSI_L0_S_OETF_POSX4			0xc81c
#define HDR_LSI_L0_S_OETF_POSX5			0xc820
#define HDR_LSI_L0_S_OETF_POSX6			0xc824
#define HDR_LSI_L0_S_OETF_POSX7			0xc828
#define HDR_LSI_L0_S_OETF_POSX8			0xc82c
#define HDR_LSI_L0_S_OETF_POSX9			0xc830
#define HDR_LSI_L0_S_OETF_POSX10		0xc834
#define HDR_LSI_L0_S_OETF_POSX11		0xc838
#define HDR_LSI_L0_S_OETF_POSX12		0xc83c
#define HDR_LSI_L0_S_OETF_POSX13		0xc840
#define HDR_LSI_L0_S_OETF_POSX14		0xc844
#define HDR_LSI_L0_S_OETF_POSX15		0xc848
#define HDR_LSI_L0_S_OETF_POSX16		0xc84c
#define HDR_LSI_L0_S_OETF_POSY0			0xc850
#define HDR_LSI_L0_S_OETF_POSY1			0xc854
#define HDR_LSI_L0_S_OETF_POSY2			0xc858
#define HDR_LSI_L0_S_OETF_POSY3			0xc85c
#define HDR_LSI_L0_S_OETF_POSY4			0xc860
#define HDR_LSI_L0_S_OETF_POSY5			0xc864
#define HDR_LSI_L0_S_OETF_POSY6			0xc868
#define HDR_LSI_L0_S_OETF_POSY7			0xc86c
#define HDR_LSI_L0_S_OETF_POSY8			0xc870
#define HDR_LSI_L0_S_OETF_POSY9			0xc874
#define HDR_LSI_L0_S_OETF_POSY10		0xc878
#define HDR_LSI_L0_S_OETF_POSY11		0xc87c
#define HDR_LSI_L0_S_OETF_POSY12		0xc880
#define HDR_LSI_L0_S_OETF_POSY13		0xc884
#define HDR_LSI_L0_S_OETF_POSY14		0xc888
#define HDR_LSI_L0_S_OETF_POSY15		0xc88c
#define HDR_LSI_L0_S_OETF_POSY16		0xc890
#define HDR_LSI_L0_S_EOTF_POSX0			0xc894
#define HDR_LSI_L0_S_EOTF_POSX1			0xc898
#define HDR_LSI_L0_S_EOTF_POSX2			0xc89c
#define HDR_LSI_L0_S_EOTF_POSX3			0xc8a0
#define HDR_LSI_L0_S_EOTF_POSX4			0xc8a4
#define HDR_LSI_L0_S_EOTF_POSX5			0xc8a8
#define HDR_LSI_L0_S_EOTF_POSX6			0xc8ac
#define HDR_LSI_L0_S_EOTF_POSX7			0xc8b0
#define HDR_LSI_L0_S_EOTF_POSX8			0xc8b4
#define HDR_LSI_L0_S_EOTF_POSX9			0xc8b8
#define HDR_LSI_L0_S_EOTF_POSX10		0xc8bc
#define HDR_LSI_L0_S_EOTF_POSX11		0xc8c0
#define HDR_LSI_L0_S_EOTF_POSX12		0xc8c4
#define HDR_LSI_L0_S_EOTF_POSX13		0xc8c8
#define HDR_LSI_L0_S_EOTF_POSX14		0xc8cc
#define HDR_LSI_L0_S_EOTF_POSX15		0xc8d0
#define HDR_LSI_L0_S_EOTF_POSX16		0xc8d4
#define HDR_LSI_L0_S_EOTF_POSX17		0xc8d8
#define HDR_LSI_L0_S_EOTF_POSX18		0xc8dc
#define HDR_LSI_L0_S_EOTF_POSX19		0xc8e0
#define HDR_LSI_L0_S_EOTF_POSX20		0xc8e4
#define HDR_LSI_L0_S_EOTF_POSX21		0xc8e8
#define HDR_LSI_L0_S_EOTF_POSX22		0xc8ec
#define HDR_LSI_L0_S_EOTF_POSX23		0xc8f0
#define HDR_LSI_L0_S_EOTF_POSX24		0xc8f4
#define HDR_LSI_L0_S_EOTF_POSX25		0xc8f8
#define HDR_LSI_L0_S_EOTF_POSX26		0xc8fc
#define HDR_LSI_L0_S_EOTF_POSX27		0xc900
#define HDR_LSI_L0_S_EOTF_POSX28		0xc904
#define HDR_LSI_L0_S_EOTF_POSX29		0xc908
#define HDR_LSI_L0_S_EOTF_POSX30		0xc90c
#define HDR_LSI_L0_S_EOTF_POSX31		0xc910
#define HDR_LSI_L0_S_EOTF_POSX32		0xc994
#define HDR_LSI_L0_S_EOTF_POSY0			0xc998
#define HDR_LSI_L0_S_EOTF_POSY1			0xc99c
#define HDR_LSI_L0_S_EOTF_POSY2			0xc9a0
#define HDR_LSI_L0_S_EOTF_POSY3			0xc9a4
#define HDR_LSI_L0_S_EOTF_POSY4			0xc9a8
#define HDR_LSI_L0_S_EOTF_POSY5			0xc9ac
#define HDR_LSI_L0_S_EOTF_POSY6			0xc9b0
#define HDR_LSI_L0_S_EOTF_POSY7			0xc9b4
#define HDR_LSI_L0_S_EOTF_POSY8			0xc9b8
#define HDR_LSI_L0_S_EOTF_POSY9			0xc9bc
#define HDR_LSI_L0_S_EOTF_POSY10		0xc9c0
#define HDR_LSI_L0_S_EOTF_POSY11		0xc9c4
#define HDR_LSI_L0_S_EOTF_POSY12		0xc9c8
#define HDR_LSI_L0_S_EOTF_POSY13		0xc9cc
#define HDR_LSI_L0_S_EOTF_POSY14		0xc9d0
#define HDR_LSI_L0_S_EOTF_POSY15		0xc9d4
#define HDR_LSI_L0_S_EOTF_POSY16		0xc9d8
#define HDR_LSI_L0_S_EOTF_POSY17		0xc9dc
#define HDR_LSI_L0_S_EOTF_POSY18		0xc9e0
#define HDR_LSI_L0_S_EOTF_POSY19		0xc9e4
#define HDR_LSI_L0_S_EOTF_POSY20		0xc9e8
#define HDR_LSI_L0_S_EOTF_POSY21		0xc9ec
#define HDR_LSI_L0_S_EOTF_POSY22		0xc9f0
#define HDR_LSI_L0_S_EOTF_POSY23		0xc9f4
#define HDR_LSI_L0_S_EOTF_POSY24		0xc9f8
#define HDR_LSI_L0_S_EOTF_POSY25		0xc9fc
#define HDR_LSI_L0_S_EOTF_POSY26		0xca00
#define HDR_LSI_L0_S_EOTF_POSY27		0xca04
#define HDR_LSI_L0_S_EOTF_POSY28		0xca08
#define HDR_LSI_L0_S_EOTF_POSY29		0xca0c
#define HDR_LSI_L0_S_EOTF_POSY30		0xca10
#define HDR_LSI_L0_S_EOTF_POSY31		0xca14
#define HDR_LSI_L0_S_EOTF_POSY32		0xca18
#define HDR_LSI_L0_S_EOTF_POSY33		0xca1c
#define HDR_LSI_L0_S_EOTF_POSY34		0xca20
#define HDR_LSI_L0_S_EOTF_POSY35		0xca24
#define HDR_LSI_L0_S_EOTF_POSY36		0xca28
#define HDR_LSI_L0_S_EOTF_POSY37		0xca2c
#define HDR_LSI_L0_S_EOTF_POSY38		0xca30
#define HDR_LSI_L0_S_EOTF_POSY39		0xca34
#define HDR_LSI_L0_S_EOTF_POSY40		0xca38
#define HDR_LSI_L0_S_EOTF_POSY41		0xca3c
#define HDR_LSI_L0_S_EOTF_POSY42		0xca40
#define HDR_LSI_L0_S_EOTF_POSY43		0xca44
#define HDR_LSI_L0_S_EOTF_POSY44		0xca48
#define HDR_LSI_L0_S_EOTF_POSY45		0xca4c
#define HDR_LSI_L0_S_EOTF_POSY46		0xca50
#define HDR_LSI_L0_S_EOTF_POSY47		0xca54
#define HDR_LSI_L0_S_EOTF_POSY48		0xca58
#define HDR_LSI_L0_S_EOTF_POSY49		0xca5c
#define HDR_LSI_L0_S_EOTF_POSY50		0xca60
#define HDR_LSI_L0_S_EOTF_POSY51		0xca64
#define HDR_LSI_L0_S_EOTF_POSY52		0xca68
#define HDR_LSI_L0_S_EOTF_POSY53		0xca6c
#define HDR_LSI_L0_S_EOTF_POSY54		0xca70
#define HDR_LSI_L0_S_EOTF_POSY55		0xca74
#define HDR_LSI_L0_S_EOTF_POSY56		0xca78
#define HDR_LSI_L0_S_EOTF_POSY57		0xca7c
#define HDR_LSI_L0_S_EOTF_POSY58		0xca80
#define HDR_LSI_L0_S_EOTF_POSY59		0xca84
#define HDR_LSI_L0_S_EOTF_POSY60		0xca88
#define HDR_LSI_L0_S_EOTF_POSY61		0xca8c
#define HDR_LSI_L0_S_EOTF_POSY62		0xca90
#define HDR_LSI_L0_S_EOTF_POSY63		0xca94
#define HDR_LSI_L0_S_EOTF_POSY64		0xcb98
#define HDR_LSI_L0_S_GM_COEF0			0xcb9c
#define HDR_LSI_L0_S_GM_COEF1			0xcba0
#define HDR_LSI_L0_S_GM_COEF2			0xcba4
#define HDR_LSI_L0_S_GM_COEF3			0xcba8
#define HDR_LSI_L0_S_GM_COEF4			0xcbac
#define HDR_LSI_L0_S_GM_COEF5			0xcbb0
#define HDR_LSI_L0_S_GM_COEF6			0xcbb4
#define HDR_LSI_L0_S_GM_COEF7			0xcbb8
#define HDR_LSI_L0_S_GM_COEF8			0xcbbc

#define HDR_LSI_L1_OETF_POSX0			0xd00c
#define HDR_LSI_L1_OETF_POSX1			0xd010
#define HDR_LSI_L1_OETF_POSX2			0xd014
#define HDR_LSI_L1_OETF_POSX3			0xd018
#define HDR_LSI_L1_OETF_POSX4			0xd01c
#define HDR_LSI_L1_OETF_POSX5			0xd020
#define HDR_LSI_L1_OETF_POSX6			0xd024
#define HDR_LSI_L1_OETF_POSX7			0xd028
#define HDR_LSI_L1_OETF_POSX8			0xd02c
#define HDR_LSI_L1_OETF_POSX9			0xd030
#define HDR_LSI_L1_OETF_POSX10			0xd034
#define HDR_LSI_L1_OETF_POSX11			0xd038
#define HDR_LSI_L1_OETF_POSX12			0xd03c
#define HDR_LSI_L1_OETF_POSX13			0xd040
#define HDR_LSI_L1_OETF_POSX14			0xd044
#define HDR_LSI_L1_OETF_POSX15			0xd048
#define HDR_LSI_L1_OETF_POSX16			0xd04c
#define HDR_LSI_L1_OETF_POSY0			0xd050
#define HDR_LSI_L1_OETF_POSY1			0xd054
#define HDR_LSI_L1_OETF_POSY2			0xd058
#define HDR_LSI_L1_OETF_POSY3			0xd05c
#define HDR_LSI_L1_OETF_POSY4			0xd060
#define HDR_LSI_L1_OETF_POSY5			0xd064
#define HDR_LSI_L1_OETF_POSY6			0xd068
#define HDR_LSI_L1_OETF_POSY7			0xd06c
#define HDR_LSI_L1_OETF_POSY8			0xd070
#define HDR_LSI_L1_OETF_POSY9			0xd074
#define HDR_LSI_L1_OETF_POSY10			0xd078
#define HDR_LSI_L1_OETF_POSY11			0xd07c
#define HDR_LSI_L1_OETF_POSY12			0xd080
#define HDR_LSI_L1_OETF_POSY13			0xd084
#define HDR_LSI_L1_OETF_POSY14			0xd088
#define HDR_LSI_L1_OETF_POSY15			0xd08c
#define HDR_LSI_L1_OETF_POSY16			0xd090
#define HDR_LSI_L1_EOTF_POSX0			0xd094
#define HDR_LSI_L1_EOTF_POSX1			0xd098
#define HDR_LSI_L1_EOTF_POSX2			0xd09c
#define HDR_LSI_L1_EOTF_POSX3			0xd0a0
#define HDR_LSI_L1_EOTF_POSX4			0xd0a4
#define HDR_LSI_L1_EOTF_POSX5			0xd0a8
#define HDR_LSI_L1_EOTF_POSX6			0xd0ac
#define HDR_LSI_L1_EOTF_POSX7			0xd0b0
#define HDR_LSI_L1_EOTF_POSX8			0xd0b4
#define HDR_LSI_L1_EOTF_POSX9			0xd0b8
#define HDR_LSI_L1_EOTF_POSX10			0xd0bc
#define HDR_LSI_L1_EOTF_POSX11			0xd0c0
#define HDR_LSI_L1_EOTF_POSX12			0xd0c4
#define HDR_LSI_L1_EOTF_POSX13			0xd0c8
#define HDR_LSI_L1_EOTF_POSX14			0xd0cc
#define HDR_LSI_L1_EOTF_POSX15			0xd0d0
#define HDR_LSI_L1_EOTF_POSX16			0xd0d4
#define HDR_LSI_L1_EOTF_POSX17			0xd0d8
#define HDR_LSI_L1_EOTF_POSX18			0xd0dc
#define HDR_LSI_L1_EOTF_POSX19			0xd0e0
#define HDR_LSI_L1_EOTF_POSX20			0xd0e4
#define HDR_LSI_L1_EOTF_POSX21			0xd0e8
#define HDR_LSI_L1_EOTF_POSX22			0xd0ec
#define HDR_LSI_L1_EOTF_POSX23			0xd0f0
#define HDR_LSI_L1_EOTF_POSX24			0xd0f4
#define HDR_LSI_L1_EOTF_POSX25			0xd0f8
#define HDR_LSI_L1_EOTF_POSX26			0xd0fc
#define HDR_LSI_L1_EOTF_POSX27			0xd100
#define HDR_LSI_L1_EOTF_POSX28			0xd104
#define HDR_LSI_L1_EOTF_POSX29			0xd108
#define HDR_LSI_L1_EOTF_POSX30			0xd10c
#define HDR_LSI_L1_EOTF_POSX31			0xd110
#define HDR_LSI_L1_EOTF_POSX32			0xd114
#define HDR_LSI_L1_EOTF_POSX33			0xd118
#define HDR_LSI_L1_EOTF_POSX34			0xd11c
#define HDR_LSI_L1_EOTF_POSX35			0xd120
#define HDR_LSI_L1_EOTF_POSX36			0xd124
#define HDR_LSI_L1_EOTF_POSX37			0xd128
#define HDR_LSI_L1_EOTF_POSX38			0xd12c
#define HDR_LSI_L1_EOTF_POSX39			0xd130
#define HDR_LSI_L1_EOTF_POSX40			0xd134
#define HDR_LSI_L1_EOTF_POSX41			0xd138
#define HDR_LSI_L1_EOTF_POSX42			0xd13c
#define HDR_LSI_L1_EOTF_POSX43			0xd140
#define HDR_LSI_L1_EOTF_POSX44			0xd144
#define HDR_LSI_L1_EOTF_POSX45			0xd148
#define HDR_LSI_L1_EOTF_POSX46			0xd14c
#define HDR_LSI_L1_EOTF_POSX47			0xd150
#define HDR_LSI_L1_EOTF_POSX48			0xd154
#define HDR_LSI_L1_EOTF_POSX49			0xd158
#define HDR_LSI_L1_EOTF_POSX50			0xd15c
#define HDR_LSI_L1_EOTF_POSX51			0xd160
#define HDR_LSI_L1_EOTF_POSX52			0xd164
#define HDR_LSI_L1_EOTF_POSX53			0xd168
#define HDR_LSI_L1_EOTF_POSX54			0xd16c
#define HDR_LSI_L1_EOTF_POSX55			0xd170
#define HDR_LSI_L1_EOTF_POSX56			0xd174
#define HDR_LSI_L1_EOTF_POSX57			0xd178
#define HDR_LSI_L1_EOTF_POSX58			0xd17c
#define HDR_LSI_L1_EOTF_POSX59			0xd180
#define HDR_LSI_L1_EOTF_POSX60			0xd184
#define HDR_LSI_L1_EOTF_POSX61			0xd188
#define HDR_LSI_L1_EOTF_POSX62			0xd18c
#define HDR_LSI_L1_EOTF_POSX63			0xd190
#define HDR_LSI_L1_EOTF_POSX64			0xd194
#define HDR_LSI_L1_EOTF_POSY0			0xd198
#define HDR_LSI_L1_EOTF_POSY1			0xd19c
#define HDR_LSI_L1_EOTF_POSY2			0xd1a0
#define HDR_LSI_L1_EOTF_POSY3			0xd1a4
#define HDR_LSI_L1_EOTF_POSY4			0xd1a8
#define HDR_LSI_L1_EOTF_POSY5			0xd1ac
#define HDR_LSI_L1_EOTF_POSY6			0xd1b0
#define HDR_LSI_L1_EOTF_POSY7			0xd1b4
#define HDR_LSI_L1_EOTF_POSY8			0xd1b8
#define HDR_LSI_L1_EOTF_POSY9			0xd1bc
#define HDR_LSI_L1_EOTF_POSY10			0xd1c0
#define HDR_LSI_L1_EOTF_POSY11			0xd1c4
#define HDR_LSI_L1_EOTF_POSY12			0xd1c8
#define HDR_LSI_L1_EOTF_POSY13			0xd1cc
#define HDR_LSI_L1_EOTF_POSY14			0xd1d0
#define HDR_LSI_L1_EOTF_POSY15			0xd1d4
#define HDR_LSI_L1_EOTF_POSY16			0xd1d8
#define HDR_LSI_L1_EOTF_POSY17			0xd1dc
#define HDR_LSI_L1_EOTF_POSY18			0xd1e0
#define HDR_LSI_L1_EOTF_POSY19			0xd1e4
#define HDR_LSI_L1_EOTF_POSY20			0xd1e8
#define HDR_LSI_L1_EOTF_POSY21			0xd1ec
#define HDR_LSI_L1_EOTF_POSY22			0xd1f0
#define HDR_LSI_L1_EOTF_POSY23			0xd1f4
#define HDR_LSI_L1_EOTF_POSY24			0xd1f8
#define HDR_LSI_L1_EOTF_POSY25			0xd1fc
#define HDR_LSI_L1_EOTF_POSY26			0xd200
#define HDR_LSI_L1_EOTF_POSY27			0xd204
#define HDR_LSI_L1_EOTF_POSY28			0xd208
#define HDR_LSI_L1_EOTF_POSY29			0xd20c
#define HDR_LSI_L1_EOTF_POSY30			0xd210
#define HDR_LSI_L1_EOTF_POSY31			0xd214
#define HDR_LSI_L1_EOTF_POSY32			0xd218
#define HDR_LSI_L1_EOTF_POSY33			0xd21c
#define HDR_LSI_L1_EOTF_POSY34			0xd220
#define HDR_LSI_L1_EOTF_POSY35			0xd224
#define HDR_LSI_L1_EOTF_POSY36			0xd228
#define HDR_LSI_L1_EOTF_POSY37			0xd22c
#define HDR_LSI_L1_EOTF_POSY38			0xd230
#define HDR_LSI_L1_EOTF_POSY39			0xd234
#define HDR_LSI_L1_EOTF_POSY40			0xd238
#define HDR_LSI_L1_EOTF_POSY41			0xd23c
#define HDR_LSI_L1_EOTF_POSY42			0xd240
#define HDR_LSI_L1_EOTF_POSY43			0xd244
#define HDR_LSI_L1_EOTF_POSY44			0xd248
#define HDR_LSI_L1_EOTF_POSY45			0xd24c
#define HDR_LSI_L1_EOTF_POSY46			0xd250
#define HDR_LSI_L1_EOTF_POSY47			0xd254
#define HDR_LSI_L1_EOTF_POSY48			0xd258
#define HDR_LSI_L1_EOTF_POSY49			0xd25c
#define HDR_LSI_L1_EOTF_POSY50			0xd260
#define HDR_LSI_L1_EOTF_POSY51			0xd264
#define HDR_LSI_L1_EOTF_POSY52			0xd268
#define HDR_LSI_L1_EOTF_POSY53			0xd26c
#define HDR_LSI_L1_EOTF_POSY54			0xd270
#define HDR_LSI_L1_EOTF_POSY55			0xd274
#define HDR_LSI_L1_EOTF_POSY56			0xd278
#define HDR_LSI_L1_EOTF_POSY57			0xd27c
#define HDR_LSI_L1_EOTF_POSY58			0xd280
#define HDR_LSI_L1_EOTF_POSY59			0xd284
#define HDR_LSI_L1_EOTF_POSY60			0xd288
#define HDR_LSI_L1_EOTF_POSY61			0xd28c
#define HDR_LSI_L1_EOTF_POSY62			0xd290
#define HDR_LSI_L1_EOTF_POSY63			0xd294
#define HDR_LSI_L1_EOTF_POSY64			0xd298
#define HDR_LSI_L1_EOTF_POSY65			0xd29c
#define HDR_LSI_L1_EOTF_POSY66			0xd2a0
#define HDR_LSI_L1_EOTF_POSY67			0xd2a4
#define HDR_LSI_L1_EOTF_POSY68			0xd2a8
#define HDR_LSI_L1_EOTF_POSY69			0xd2ac
#define HDR_LSI_L1_EOTF_POSY70			0xd2b0
#define HDR_LSI_L1_EOTF_POSY71			0xd2b4
#define HDR_LSI_L1_EOTF_POSY72			0xd2b8
#define HDR_LSI_L1_EOTF_POSY73			0xd2bc
#define HDR_LSI_L1_EOTF_POSY74			0xd2c0
#define HDR_LSI_L1_EOTF_POSY75			0xd2c4
#define HDR_LSI_L1_EOTF_POSY76			0xd2c8
#define HDR_LSI_L1_EOTF_POSY77			0xd2cc
#define HDR_LSI_L1_EOTF_POSY78			0xd2d0
#define HDR_LSI_L1_EOTF_POSY79			0xd2d4
#define HDR_LSI_L1_EOTF_POSY80			0xd2d8
#define HDR_LSI_L1_EOTF_POSY81			0xd2dc
#define HDR_LSI_L1_EOTF_POSY82			0xd2e0
#define HDR_LSI_L1_EOTF_POSY83			0xd2e4
#define HDR_LSI_L1_EOTF_POSY84			0xd2e8
#define HDR_LSI_L1_EOTF_POSY85			0xd2ec
#define HDR_LSI_L1_EOTF_POSY86			0xd2f0
#define HDR_LSI_L1_EOTF_POSY87			0xd2f4
#define HDR_LSI_L1_EOTF_POSY88			0xd2f8
#define HDR_LSI_L1_EOTF_POSY89			0xd2fc
#define HDR_LSI_L1_EOTF_POSY90			0xd300
#define HDR_LSI_L1_EOTF_POSY91			0xd304
#define HDR_LSI_L1_EOTF_POSY92			0xd308
#define HDR_LSI_L1_EOTF_POSY93			0xd30c
#define HDR_LSI_L1_EOTF_POSY94			0xd310
#define HDR_LSI_L1_EOTF_POSY95			0xd314
#define HDR_LSI_L1_EOTF_POSY96			0xd318
#define HDR_LSI_L1_EOTF_POSY97			0xd31c
#define HDR_LSI_L1_EOTF_POSY98			0xd320
#define HDR_LSI_L1_EOTF_POSY99			0xd324
#define HDR_LSI_L1_EOTF_POSY100			0xd328
#define HDR_LSI_L1_EOTF_POSY101			0xd32c
#define HDR_LSI_L1_EOTF_POSY102			0xd330
#define HDR_LSI_L1_EOTF_POSY103			0xd334
#define HDR_LSI_L1_EOTF_POSY104			0xd338
#define HDR_LSI_L1_EOTF_POSY105			0xd33c
#define HDR_LSI_L1_EOTF_POSY106			0xd340
#define HDR_LSI_L1_EOTF_POSY107			0xd344
#define HDR_LSI_L1_EOTF_POSY108			0xd348
#define HDR_LSI_L1_EOTF_POSY109			0xd34c
#define HDR_LSI_L1_EOTF_POSY110			0xd350
#define HDR_LSI_L1_EOTF_POSY111			0xd354
#define HDR_LSI_L1_EOTF_POSY112			0xd358
#define HDR_LSI_L1_EOTF_POSY113			0xd35c
#define HDR_LSI_L1_EOTF_POSY114			0xd360
#define HDR_LSI_L1_EOTF_POSY115			0xd364
#define HDR_LSI_L1_EOTF_POSY116			0xd368
#define HDR_LSI_L1_EOTF_POSY117			0xd36c
#define HDR_LSI_L1_EOTF_POSY118			0xd370
#define HDR_LSI_L1_EOTF_POSY119			0xd374
#define HDR_LSI_L1_EOTF_POSY120			0xd378
#define HDR_LSI_L1_EOTF_POSY121			0xd37c
#define HDR_LSI_L1_EOTF_POSY122			0xd380
#define HDR_LSI_L1_EOTF_POSY123			0xd384
#define HDR_LSI_L1_EOTF_POSY124			0xd388
#define HDR_LSI_L1_EOTF_POSY125			0xd38c
#define HDR_LSI_L1_EOTF_POSY126			0xd390
#define HDR_LSI_L1_EOTF_POSY127			0xd394
#define HDR_LSI_L1_EOTF_POSY128			0xd398
#define HDR_LSI_L1_GM_COEF0			0xd39c
#define HDR_LSI_L1_GM_COEF1			0xd3a0
#define HDR_LSI_L1_GM_COEF2			0xd3a4
#define HDR_LSI_L1_GM_COEF3			0xd3a8
#define HDR_LSI_L1_GM_COEF4			0xd3ac
#define HDR_LSI_L1_GM_COEF5			0xd3b0
#define HDR_LSI_L1_GM_COEF6			0xd3b4
#define HDR_LSI_L1_GM_COEF7			0xd3b8
#define HDR_LSI_L1_GM_COEF8			0xd3bc
#define HDR_LSI_L1_S_OETF_POSX0			0xd80c
#define HDR_LSI_L1_S_OETF_POSX1			0xd810
#define HDR_LSI_L1_S_OETF_POSX2			0xd814
#define HDR_LSI_L1_S_OETF_POSX3			0xd818
#define HDR_LSI_L1_S_OETF_POSX4			0xd81c
#define HDR_LSI_L1_S_OETF_POSX5			0xd820
#define HDR_LSI_L1_S_OETF_POSX6			0xd824
#define HDR_LSI_L1_S_OETF_POSX7			0xd828
#define HDR_LSI_L1_S_OETF_POSX8			0xd82c
#define HDR_LSI_L1_S_OETF_POSX9			0xd830
#define HDR_LSI_L1_S_OETF_POSX10		0xd834
#define HDR_LSI_L1_S_OETF_POSX11		0xd838
#define HDR_LSI_L1_S_OETF_POSX12		0xd83c
#define HDR_LSI_L1_S_OETF_POSX13		0xd840
#define HDR_LSI_L1_S_OETF_POSX14		0xd844
#define HDR_LSI_L1_S_OETF_POSX15		0xd848
#define HDR_LSI_L1_S_OETF_POSX16		0xd84c
#define HDR_LSI_L1_S_OETF_POSY0			0xd850
#define HDR_LSI_L1_S_OETF_POSY1			0xd854
#define HDR_LSI_L1_S_OETF_POSY2			0xd858
#define HDR_LSI_L1_S_OETF_POSY3			0xd85c
#define HDR_LSI_L1_S_OETF_POSY4			0xd860
#define HDR_LSI_L1_S_OETF_POSY5			0xd864
#define HDR_LSI_L1_S_OETF_POSY6			0xd868
#define HDR_LSI_L1_S_OETF_POSY7			0xd86c
#define HDR_LSI_L1_S_OETF_POSY8			0xd870
#define HDR_LSI_L1_S_OETF_POSY9			0xd874
#define HDR_LSI_L1_S_OETF_POSY10		0xd878
#define HDR_LSI_L1_S_OETF_POSY11		0xd87c
#define HDR_LSI_L1_S_OETF_POSY12		0xd880
#define HDR_LSI_L1_S_OETF_POSY13		0xd884
#define HDR_LSI_L1_S_OETF_POSY14		0xd888
#define HDR_LSI_L1_S_OETF_POSY15		0xd88c
#define HDR_LSI_L1_S_OETF_POSY16		0xd890
#define HDR_LSI_L1_S_EOTF_POSX0			0xd894
#define HDR_LSI_L1_S_EOTF_POSX1			0xd898
#define HDR_LSI_L1_S_EOTF_POSX2			0xd89c
#define HDR_LSI_L1_S_EOTF_POSX3			0xd8a0
#define HDR_LSI_L1_S_EOTF_POSX4			0xd8a4
#define HDR_LSI_L1_S_EOTF_POSX5			0xd8a8
#define HDR_LSI_L1_S_EOTF_POSX6			0xd8ac
#define HDR_LSI_L1_S_EOTF_POSX7			0xd8b0
#define HDR_LSI_L1_S_EOTF_POSX8			0xd8b4
#define HDR_LSI_L1_S_EOTF_POSX9			0xd8b8
#define HDR_LSI_L1_S_EOTF_POSX10		0xd8bc
#define HDR_LSI_L1_S_EOTF_POSX11		0xd8c0
#define HDR_LSI_L1_S_EOTF_POSX12		0xd8c4
#define HDR_LSI_L1_S_EOTF_POSX13		0xd8c8
#define HDR_LSI_L1_S_EOTF_POSX14		0xd8cc
#define HDR_LSI_L1_S_EOTF_POSX15		0xd8d0
#define HDR_LSI_L1_S_EOTF_POSX16		0xd8d4
#define HDR_LSI_L1_S_EOTF_POSX17		0xd8d8
#define HDR_LSI_L1_S_EOTF_POSX18		0xd8dc
#define HDR_LSI_L1_S_EOTF_POSX19		0xd8e0
#define HDR_LSI_L1_S_EOTF_POSX20		0xd8e4
#define HDR_LSI_L1_S_EOTF_POSX21		0xd8e8
#define HDR_LSI_L1_S_EOTF_POSX22		0xd8ec
#define HDR_LSI_L1_S_EOTF_POSX23		0xd8f0
#define HDR_LSI_L1_S_EOTF_POSX24		0xd8f4
#define HDR_LSI_L1_S_EOTF_POSX25		0xd8f8
#define HDR_LSI_L1_S_EOTF_POSX26		0xd8fc
#define HDR_LSI_L1_S_EOTF_POSX27		0xd900
#define HDR_LSI_L1_S_EOTF_POSX28		0xd904
#define HDR_LSI_L1_S_EOTF_POSX29		0xd908
#define HDR_LSI_L1_S_EOTF_POSX30		0xd90c
#define HDR_LSI_L1_S_EOTF_POSX31		0xd910
#define HDR_LSI_L1_S_EOTF_POSX32		0xd994
#define HDR_LSI_L1_S_EOTF_POSY0			0xd998
#define HDR_LSI_L1_S_EOTF_POSY1			0xd99c
#define HDR_LSI_L1_S_EOTF_POSY2			0xd9a0
#define HDR_LSI_L1_S_EOTF_POSY3			0xd9a4
#define HDR_LSI_L1_S_EOTF_POSY4			0xd9a8
#define HDR_LSI_L1_S_EOTF_POSY5			0xd9ac
#define HDR_LSI_L1_S_EOTF_POSY6			0xd9b0
#define HDR_LSI_L1_S_EOTF_POSY7			0xd9b4
#define HDR_LSI_L1_S_EOTF_POSY8			0xd9b8
#define HDR_LSI_L1_S_EOTF_POSY9			0xd9bc
#define HDR_LSI_L1_S_EOTF_POSY10		0xd9c0
#define HDR_LSI_L1_S_EOTF_POSY11		0xd9c4
#define HDR_LSI_L1_S_EOTF_POSY12		0xd9c8
#define HDR_LSI_L1_S_EOTF_POSY13		0xd9cc
#define HDR_LSI_L1_S_EOTF_POSY14		0xd9d0
#define HDR_LSI_L1_S_EOTF_POSY15		0xd9d4
#define HDR_LSI_L1_S_EOTF_POSY16		0xd9d8
#define HDR_LSI_L1_S_EOTF_POSY17		0xd9dc
#define HDR_LSI_L1_S_EOTF_POSY18		0xd9e0
#define HDR_LSI_L1_S_EOTF_POSY19		0xd9e4
#define HDR_LSI_L1_S_EOTF_POSY20		0xd9e8
#define HDR_LSI_L1_S_EOTF_POSY21		0xd9ec
#define HDR_LSI_L1_S_EOTF_POSY22		0xd9f0
#define HDR_LSI_L1_S_EOTF_POSY23		0xd9f4
#define HDR_LSI_L1_S_EOTF_POSY24		0xd9f8
#define HDR_LSI_L1_S_EOTF_POSY25		0xd9fc
#define HDR_LSI_L1_S_EOTF_POSY26		0xda00
#define HDR_LSI_L1_S_EOTF_POSY27		0xda04
#define HDR_LSI_L1_S_EOTF_POSY28		0xda08
#define HDR_LSI_L1_S_EOTF_POSY29		0xda0c
#define HDR_LSI_L1_S_EOTF_POSY30		0xda10
#define HDR_LSI_L1_S_EOTF_POSY31		0xda14
#define HDR_LSI_L1_S_EOTF_POSY32		0xda18
#define HDR_LSI_L1_S_EOTF_POSY33		0xda1c
#define HDR_LSI_L1_S_EOTF_POSY34		0xda20
#define HDR_LSI_L1_S_EOTF_POSY35		0xda24
#define HDR_LSI_L1_S_EOTF_POSY36		0xda28
#define HDR_LSI_L1_S_EOTF_POSY37		0xda2c
#define HDR_LSI_L1_S_EOTF_POSY38		0xda30
#define HDR_LSI_L1_S_EOTF_POSY39		0xda34
#define HDR_LSI_L1_S_EOTF_POSY40		0xda38
#define HDR_LSI_L1_S_EOTF_POSY41		0xda3c
#define HDR_LSI_L1_S_EOTF_POSY42		0xda40
#define HDR_LSI_L1_S_EOTF_POSY43		0xda44
#define HDR_LSI_L1_S_EOTF_POSY44		0xda48
#define HDR_LSI_L1_S_EOTF_POSY45		0xda4c
#define HDR_LSI_L1_S_EOTF_POSY46		0xda50
#define HDR_LSI_L1_S_EOTF_POSY47		0xda54
#define HDR_LSI_L1_S_EOTF_POSY48		0xda58
#define HDR_LSI_L1_S_EOTF_POSY49		0xda5c
#define HDR_LSI_L1_S_EOTF_POSY50		0xda60
#define HDR_LSI_L1_S_EOTF_POSY51		0xda64
#define HDR_LSI_L1_S_EOTF_POSY52		0xda68
#define HDR_LSI_L1_S_EOTF_POSY53		0xda6c
#define HDR_LSI_L1_S_EOTF_POSY54		0xda70
#define HDR_LSI_L1_S_EOTF_POSY55		0xda74
#define HDR_LSI_L1_S_EOTF_POSY56		0xda78
#define HDR_LSI_L1_S_EOTF_POSY57		0xda7c
#define HDR_LSI_L1_S_EOTF_POSY58		0xda80
#define HDR_LSI_L1_S_EOTF_POSY59		0xda84
#define HDR_LSI_L1_S_EOTF_POSY60		0xda88
#define HDR_LSI_L1_S_EOTF_POSY61		0xda8c
#define HDR_LSI_L1_S_EOTF_POSY62		0xda90
#define HDR_LSI_L1_S_EOTF_POSY63		0xda94
#define HDR_LSI_L1_S_EOTF_POSY64		0xdb98
#define HDR_LSI_L1_S_GM_COEF0			0xdb9c
#define HDR_LSI_L1_S_GM_COEF1			0xdba0
#define HDR_LSI_L1_S_GM_COEF2			0xdba4
#define HDR_LSI_L1_S_GM_COEF3			0xdba8
#define HDR_LSI_L1_S_GM_COEF4			0xdbac
#define HDR_LSI_L1_S_GM_COEF5			0xdbb0
#define HDR_LSI_L1_S_GM_COEF6			0xdbb4
#define HDR_LSI_L1_S_GM_COEF7			0xdbb8
#define HDR_LSI_L1_S_GM_COEF8			0xdbbc

#define HDR_LSI_L2_OETF_POSX0		0xe00c
#define HDR_LSI_L2_OETF_POSX1		0xe010
#define HDR_LSI_L2_OETF_POSX2		0xe014
#define HDR_LSI_L2_OETF_POSX3		0xe018
#define HDR_LSI_L2_OETF_POSX4		0xe01c
#define HDR_LSI_L2_OETF_POSX5		0xe020
#define HDR_LSI_L2_OETF_POSX6		0xe024
#define HDR_LSI_L2_OETF_POSX7		0xe028
#define HDR_LSI_L2_OETF_POSX8		0xe02c
#define HDR_LSI_L2_OETF_POSX9		0xe030
#define HDR_LSI_L2_OETF_POSX10		0xe034
#define HDR_LSI_L2_OETF_POSX11		0xe038
#define HDR_LSI_L2_OETF_POSX12		0xe03c
#define HDR_LSI_L2_OETF_POSX13		0xe040
#define HDR_LSI_L2_OETF_POSX14		0xe044
#define HDR_LSI_L2_OETF_POSX15		0xe048
#define HDR_LSI_L2_OETF_POSX16		0xe04c
#define HDR_LSI_L2_OETF_POSY0		0xe050
#define HDR_LSI_L2_OETF_POSY1		0xe054
#define HDR_LSI_L2_OETF_POSY2		0xe058
#define HDR_LSI_L2_OETF_POSY3		0xe05c
#define HDR_LSI_L2_OETF_POSY4		0xe060
#define HDR_LSI_L2_OETF_POSY5		0xe064
#define HDR_LSI_L2_OETF_POSY6		0xe068
#define HDR_LSI_L2_OETF_POSY7		0xe06c
#define HDR_LSI_L2_OETF_POSY8		0xe070
#define HDR_LSI_L2_OETF_POSY9		0xe074
#define HDR_LSI_L2_OETF_POSY10		0xe078
#define HDR_LSI_L2_OETF_POSY11		0xe07c
#define HDR_LSI_L2_OETF_POSY12		0xe080
#define HDR_LSI_L2_OETF_POSY13		0xe084
#define HDR_LSI_L2_OETF_POSY14		0xe088
#define HDR_LSI_L2_OETF_POSY15		0xe08c
#define HDR_LSI_L2_OETF_POSY16		0xe090
#define HDR_LSI_L2_EOTF_POSX0		0xe094
#define HDR_LSI_L2_EOTF_POSX1		0xe098
#define HDR_LSI_L2_EOTF_POSX2		0xe09c
#define HDR_LSI_L2_EOTF_POSX3		0xe0a0
#define HDR_LSI_L2_EOTF_POSX4		0xe0a4
#define HDR_LSI_L2_EOTF_POSX5		0xe0a8
#define HDR_LSI_L2_EOTF_POSX6		0xe0ac
#define HDR_LSI_L2_EOTF_POSX7		0xe0b0
#define HDR_LSI_L2_EOTF_POSX8		0xe0b4
#define HDR_LSI_L2_EOTF_POSX9		0xe0b8
#define HDR_LSI_L2_EOTF_POSX10		0xe0bc
#define HDR_LSI_L2_EOTF_POSX11		0xe0c0
#define HDR_LSI_L2_EOTF_POSX12		0xe0c4
#define HDR_LSI_L2_EOTF_POSX13		0xe0c8
#define HDR_LSI_L2_EOTF_POSX14		0xe0cc
#define HDR_LSI_L2_EOTF_POSX15		0xe0d0
#define HDR_LSI_L2_EOTF_POSX16		0xe0d4
#define HDR_LSI_L2_EOTF_POSX17		0xe0d8
#define HDR_LSI_L2_EOTF_POSX18		0xe0dc
#define HDR_LSI_L2_EOTF_POSX19		0xe0e0
#define HDR_LSI_L2_EOTF_POSX20		0xe0e4
#define HDR_LSI_L2_EOTF_POSX21		0xe0e8
#define HDR_LSI_L2_EOTF_POSX22		0xe0ec
#define HDR_LSI_L2_EOTF_POSX23		0xe0f0
#define HDR_LSI_L2_EOTF_POSX24		0xe0f4
#define HDR_LSI_L2_EOTF_POSX25		0xe0f8
#define HDR_LSI_L2_EOTF_POSX26		0xe0fc
#define HDR_LSI_L2_EOTF_POSX27		0xe100
#define HDR_LSI_L2_EOTF_POSX28		0xe104
#define HDR_LSI_L2_EOTF_POSX29		0xe108
#define HDR_LSI_L2_EOTF_POSX30		0xe10c
#define HDR_LSI_L2_EOTF_POSX31		0xe110
#define HDR_LSI_L2_EOTF_POSX32		0xe114
#define HDR_LSI_L2_EOTF_POSX33		0xe118
#define HDR_LSI_L2_EOTF_POSX34		0xe11c
#define HDR_LSI_L2_EOTF_POSX35		0xe120
#define HDR_LSI_L2_EOTF_POSX36		0xe124
#define HDR_LSI_L2_EOTF_POSX37		0xe128
#define HDR_LSI_L2_EOTF_POSX38		0xe12c
#define HDR_LSI_L2_EOTF_POSX39		0xe130
#define HDR_LSI_L2_EOTF_POSX40		0xe134
#define HDR_LSI_L2_EOTF_POSX41		0xe138
#define HDR_LSI_L2_EOTF_POSX42		0xe13c
#define HDR_LSI_L2_EOTF_POSX43		0xe140
#define HDR_LSI_L2_EOTF_POSX44		0xe144
#define HDR_LSI_L2_EOTF_POSX45		0xe148
#define HDR_LSI_L2_EOTF_POSX46		0xe14c
#define HDR_LSI_L2_EOTF_POSX47		0xe150
#define HDR_LSI_L2_EOTF_POSX48		0xe154
#define HDR_LSI_L2_EOTF_POSX49		0xe158
#define HDR_LSI_L2_EOTF_POSX50		0xe15c
#define HDR_LSI_L2_EOTF_POSX51		0xe160
#define HDR_LSI_L2_EOTF_POSX52		0xe164
#define HDR_LSI_L2_EOTF_POSX53		0xe168
#define HDR_LSI_L2_EOTF_POSX54		0xe16c
#define HDR_LSI_L2_EOTF_POSX55		0xe170
#define HDR_LSI_L2_EOTF_POSX56		0xe174
#define HDR_LSI_L2_EOTF_POSX57		0xe178
#define HDR_LSI_L2_EOTF_POSX58		0xe17c
#define HDR_LSI_L2_EOTF_POSX59		0xe180
#define HDR_LSI_L2_EOTF_POSX60		0xe184
#define HDR_LSI_L2_EOTF_POSX61		0xe188
#define HDR_LSI_L2_EOTF_POSX62		0xe18c
#define HDR_LSI_L2_EOTF_POSX63		0xe190
#define HDR_LSI_L2_EOTF_POSX64		0xe194
#define HDR_LSI_L2_EOTF_POSY0		0xe198
#define HDR_LSI_L2_EOTF_POSY1		0xe19c
#define HDR_LSI_L2_EOTF_POSY2		0xe1a0
#define HDR_LSI_L2_EOTF_POSY3		0xe1a4
#define HDR_LSI_L2_EOTF_POSY4		0xe1a8
#define HDR_LSI_L2_EOTF_POSY5		0xe1ac
#define HDR_LSI_L2_EOTF_POSY6		0xe1b0
#define HDR_LSI_L2_EOTF_POSY7		0xe1b4
#define HDR_LSI_L2_EOTF_POSY8		0xe1b8
#define HDR_LSI_L2_EOTF_POSY9		0xe1bc
#define HDR_LSI_L2_EOTF_POSY10		0xe1c0
#define HDR_LSI_L2_EOTF_POSY11		0xe1c4
#define HDR_LSI_L2_EOTF_POSY12		0xe1c8
#define HDR_LSI_L2_EOTF_POSY13		0xe1cc
#define HDR_LSI_L2_EOTF_POSY14		0xe1d0
#define HDR_LSI_L2_EOTF_POSY15		0xe1d4
#define HDR_LSI_L2_EOTF_POSY16		0xe1d8
#define HDR_LSI_L2_EOTF_POSY17		0xe1dc
#define HDR_LSI_L2_EOTF_POSY18		0xe1e0
#define HDR_LSI_L2_EOTF_POSY19		0xe1e4
#define HDR_LSI_L2_EOTF_POSY20		0xe1e8
#define HDR_LSI_L2_EOTF_POSY21		0xe1ec
#define HDR_LSI_L2_EOTF_POSY22		0xe1f0
#define HDR_LSI_L2_EOTF_POSY23		0xe1f4
#define HDR_LSI_L2_EOTF_POSY24		0xe1f8
#define HDR_LSI_L2_EOTF_POSY25		0xe1fc
#define HDR_LSI_L2_EOTF_POSY26		0xe200
#define HDR_LSI_L2_EOTF_POSY27		0xe204
#define HDR_LSI_L2_EOTF_POSY28		0xe208
#define HDR_LSI_L2_EOTF_POSY29		0xe20c
#define HDR_LSI_L2_EOTF_POSY30		0xe210
#define HDR_LSI_L2_EOTF_POSY31		0xe214
#define HDR_LSI_L2_EOTF_POSY32		0xe218
#define HDR_LSI_L2_EOTF_POSY33		0xe21c
#define HDR_LSI_L2_EOTF_POSY34		0xe220
#define HDR_LSI_L2_EOTF_POSY35		0xe224
#define HDR_LSI_L2_EOTF_POSY36		0xe228
#define HDR_LSI_L2_EOTF_POSY37		0xe22c
#define HDR_LSI_L2_EOTF_POSY38		0xe230
#define HDR_LSI_L2_EOTF_POSY39		0xe234
#define HDR_LSI_L2_EOTF_POSY40		0xe238
#define HDR_LSI_L2_EOTF_POSY41		0xe23c
#define HDR_LSI_L2_EOTF_POSY42		0xe240
#define HDR_LSI_L2_EOTF_POSY43		0xe244
#define HDR_LSI_L2_EOTF_POSY44		0xe248
#define HDR_LSI_L2_EOTF_POSY45		0xe24c
#define HDR_LSI_L2_EOTF_POSY46		0xe250
#define HDR_LSI_L2_EOTF_POSY47		0xe254
#define HDR_LSI_L2_EOTF_POSY48		0xe258
#define HDR_LSI_L2_EOTF_POSY49		0xe25c
#define HDR_LSI_L2_EOTF_POSY50		0xe260
#define HDR_LSI_L2_EOTF_POSY51		0xe264
#define HDR_LSI_L2_EOTF_POSY52		0xe268
#define HDR_LSI_L2_EOTF_POSY53		0xe26c
#define HDR_LSI_L2_EOTF_POSY54		0xe270
#define HDR_LSI_L2_EOTF_POSY55		0xe274
#define HDR_LSI_L2_EOTF_POSY56		0xe278
#define HDR_LSI_L2_EOTF_POSY57		0xe27c
#define HDR_LSI_L2_EOTF_POSY58		0xe280
#define HDR_LSI_L2_EOTF_POSY59		0xe284
#define HDR_LSI_L2_EOTF_POSY60		0xe288
#define HDR_LSI_L2_EOTF_POSY61		0xe28c
#define HDR_LSI_L2_EOTF_POSY62		0xe290
#define HDR_LSI_L2_EOTF_POSY63		0xe294
#define HDR_LSI_L2_EOTF_POSY64		0xe298
#define HDR_LSI_L2_EOTF_POSY65		0xe29c
#define HDR_LSI_L2_EOTF_POSY66		0xe2a0
#define HDR_LSI_L2_EOTF_POSY67		0xe2a4
#define HDR_LSI_L2_EOTF_POSY68		0xe2a8
#define HDR_LSI_L2_EOTF_POSY69		0xe2ac
#define HDR_LSI_L2_EOTF_POSY70		0xe2b0
#define HDR_LSI_L2_EOTF_POSY71		0xe2b4
#define HDR_LSI_L2_EOTF_POSY72		0xe2b8
#define HDR_LSI_L2_EOTF_POSY73		0xe2bc
#define HDR_LSI_L2_EOTF_POSY74		0xe2c0
#define HDR_LSI_L2_EOTF_POSY75		0xe2c4
#define HDR_LSI_L2_EOTF_POSY76		0xe2c8
#define HDR_LSI_L2_EOTF_POSY77		0xe2cc
#define HDR_LSI_L2_EOTF_POSY78		0xe2d0
#define HDR_LSI_L2_EOTF_POSY79		0xe2d4
#define HDR_LSI_L2_EOTF_POSY80		0xe2d8
#define HDR_LSI_L2_EOTF_POSY81		0xe2dc
#define HDR_LSI_L2_EOTF_POSY82		0xe2e0
#define HDR_LSI_L2_EOTF_POSY83		0xe2e4
#define HDR_LSI_L2_EOTF_POSY84		0xe2e8
#define HDR_LSI_L2_EOTF_POSY85		0xe2ec
#define HDR_LSI_L2_EOTF_POSY86		0xe2f0
#define HDR_LSI_L2_EOTF_POSY87		0xe2f4
#define HDR_LSI_L2_EOTF_POSY88		0xe2f8
#define HDR_LSI_L2_EOTF_POSY89		0xe2fc
#define HDR_LSI_L2_EOTF_POSY90		0xe300
#define HDR_LSI_L2_EOTF_POSY91		0xe304
#define HDR_LSI_L2_EOTF_POSY92		0xe308
#define HDR_LSI_L2_EOTF_POSY93		0xe30c
#define HDR_LSI_L2_EOTF_POSY94		0xe310
#define HDR_LSI_L2_EOTF_POSY95		0xe314
#define HDR_LSI_L2_EOTF_POSY96		0xe318
#define HDR_LSI_L2_EOTF_POSY97		0xe31c
#define HDR_LSI_L2_EOTF_POSY98		0xe320
#define HDR_LSI_L2_EOTF_POSY99		0xe324
#define HDR_LSI_L2_EOTF_POSY100		0xe328
#define HDR_LSI_L2_EOTF_POSY101		0xe32c
#define HDR_LSI_L2_EOTF_POSY102		0xe330
#define HDR_LSI_L2_EOTF_POSY103		0xe334
#define HDR_LSI_L2_EOTF_POSY104		0xe338
#define HDR_LSI_L2_EOTF_POSY105		0xe33c
#define HDR_LSI_L2_EOTF_POSY106		0xe340
#define HDR_LSI_L2_EOTF_POSY107		0xe344
#define HDR_LSI_L2_EOTF_POSY108		0xe348
#define HDR_LSI_L2_EOTF_POSY109		0xe34c
#define HDR_LSI_L2_EOTF_POSY110		0xe350
#define HDR_LSI_L2_EOTF_POSY111		0xe354
#define HDR_LSI_L2_EOTF_POSY112		0xe358
#define HDR_LSI_L2_EOTF_POSY113		0xe35c
#define HDR_LSI_L2_EOTF_POSY114		0xe360
#define HDR_LSI_L2_EOTF_POSY115		0xe364
#define HDR_LSI_L2_EOTF_POSY116		0xe368
#define HDR_LSI_L2_EOTF_POSY117		0xe36c
#define HDR_LSI_L2_EOTF_POSY118		0xe370
#define HDR_LSI_L2_EOTF_POSY119		0xe374
#define HDR_LSI_L2_EOTF_POSY120		0xe378
#define HDR_LSI_L2_EOTF_POSY121		0xe37c
#define HDR_LSI_L2_EOTF_POSY122		0xe380
#define HDR_LSI_L2_EOTF_POSY123		0xe384
#define HDR_LSI_L2_EOTF_POSY124		0xe388
#define HDR_LSI_L2_EOTF_POSY125		0xe38c
#define HDR_LSI_L2_EOTF_POSY126		0xe390
#define HDR_LSI_L2_EOTF_POSY127		0xe394
#define HDR_LSI_L2_EOTF_POSY128		0xe398
#define HDR_LSI_L2_GM_COEF0		0xe39c
#define HDR_LSI_L2_GM_COEF1		0xe3a0
#define HDR_LSI_L2_GM_COEF2		0xe3a4
#define HDR_LSI_L2_GM_COEF3		0xe3a8
#define HDR_LSI_L2_GM_COEF4		0xe3ac
#define HDR_LSI_L2_GM_COEF5		0xe3b0
#define HDR_LSI_L2_GM_COEF6		0xe3b4
#define HDR_LSI_L2_GM_COEF7		0xe3b8
#define HDR_LSI_L2_GM_COEF8		0xe3bc
#define HDR_LSI_L2_TS_GAIN		0xe3c0
#define HDR_LSI_L2_S_COM_CTRL		0xe804
#define HDR_LSI_L2_S_MOD_CTRL		0xe808
#define HDR_LSI_L2_S_OETF_POSX0		0xe80c
#define HDR_LSI_L2_S_OETF_POSX1		0xe810
#define HDR_LSI_L2_S_OETF_POSX2		0xe814
#define HDR_LSI_L2_S_OETF_POSX3		0xe818
#define HDR_LSI_L2_S_OETF_POSX4		0xe81c
#define HDR_LSI_L2_S_OETF_POSX5		0xe820
#define HDR_LSI_L2_S_OETF_POSX6		0xe824
#define HDR_LSI_L2_S_OETF_POSX7		0xe828
#define HDR_LSI_L2_S_OETF_POSX8		0xe82c
#define HDR_LSI_L2_S_OETF_POSX9		0xe830
#define HDR_LSI_L2_S_OETF_POSX10		0xe834
#define HDR_LSI_L2_S_OETF_POSX11		0xe838
#define HDR_LSI_L2_S_OETF_POSX12		0xe83c
#define HDR_LSI_L2_S_OETF_POSX13		0xe840
#define HDR_LSI_L2_S_OETF_POSX14		0xe844
#define HDR_LSI_L2_S_OETF_POSX15		0xe848
#define HDR_LSI_L2_S_OETF_POSX16		0xe84c
#define HDR_LSI_L2_S_OETF_POSY0		0xe850
#define HDR_LSI_L2_S_OETF_POSY1		0xe854
#define HDR_LSI_L2_S_OETF_POSY2		0xe858
#define HDR_LSI_L2_S_OETF_POSY3		0xe85c
#define HDR_LSI_L2_S_OETF_POSY4		0xe860
#define HDR_LSI_L2_S_OETF_POSY5		0xe864
#define HDR_LSI_L2_S_OETF_POSY6		0xe868
#define HDR_LSI_L2_S_OETF_POSY7		0xe86c
#define HDR_LSI_L2_S_OETF_POSY8		0xe870
#define HDR_LSI_L2_S_OETF_POSY9		0xe874
#define HDR_LSI_L2_S_OETF_POSY10		0xe878
#define HDR_LSI_L2_S_OETF_POSY11		0xe87c
#define HDR_LSI_L2_S_OETF_POSY12		0xe880
#define HDR_LSI_L2_S_OETF_POSY13		0xe884
#define HDR_LSI_L2_S_OETF_POSY14		0xe888
#define HDR_LSI_L2_S_OETF_POSY15		0xe88c
#define HDR_LSI_L2_S_OETF_POSY16		0xe890
#define HDR_LSI_L2_S_EOTF_POSX0		0xe894
#define HDR_LSI_L2_S_EOTF_POSX1		0xe898
#define HDR_LSI_L2_S_EOTF_POSX2		0xe89c
#define HDR_LSI_L2_S_EOTF_POSX3		0xe8a0
#define HDR_LSI_L2_S_EOTF_POSX4		0xe8a4
#define HDR_LSI_L2_S_EOTF_POSX5		0xe8a8
#define HDR_LSI_L2_S_EOTF_POSX6		0xe8ac
#define HDR_LSI_L2_S_EOTF_POSX7		0xe8b0
#define HDR_LSI_L2_S_EOTF_POSX8		0xe8b4
#define HDR_LSI_L2_S_EOTF_POSX9		0xe8b8
#define HDR_LSI_L2_S_EOTF_POSX10		0xe8bc
#define HDR_LSI_L2_S_EOTF_POSX11		0xe8c0
#define HDR_LSI_L2_S_EOTF_POSX12		0xe8c4
#define HDR_LSI_L2_S_EOTF_POSX13		0xe8c8
#define HDR_LSI_L2_S_EOTF_POSX14		0xe8cc
#define HDR_LSI_L2_S_EOTF_POSX15		0xe8d0
#define HDR_LSI_L2_S_EOTF_POSX16		0xe8d4
#define HDR_LSI_L2_S_EOTF_POSX17		0xe8d8
#define HDR_LSI_L2_S_EOTF_POSX18		0xe8dc
#define HDR_LSI_L2_S_EOTF_POSX19		0xe8e0
#define HDR_LSI_L2_S_EOTF_POSX20		0xe8e4
#define HDR_LSI_L2_S_EOTF_POSX21		0xe8e8
#define HDR_LSI_L2_S_EOTF_POSX22		0xe8ec
#define HDR_LSI_L2_S_EOTF_POSX23		0xe8f0
#define HDR_LSI_L2_S_EOTF_POSX24		0xe8f4
#define HDR_LSI_L2_S_EOTF_POSX25		0xe8f8
#define HDR_LSI_L2_S_EOTF_POSX26		0xe8fc
#define HDR_LSI_L2_S_EOTF_POSX27		0xe900
#define HDR_LSI_L2_S_EOTF_POSX28		0xe904
#define HDR_LSI_L2_S_EOTF_POSX29		0xe908
#define HDR_LSI_L2_S_EOTF_POSX30		0xe90c
#define HDR_LSI_L2_S_EOTF_POSX31		0xe910
#define HDR_LSI_L2_S_EOTF_POSX32		0xe994
#define HDR_LSI_L2_S_EOTF_POSY0		0xe998
#define HDR_LSI_L2_S_EOTF_POSY1		0xe99c
#define HDR_LSI_L2_S_EOTF_POSY2		0xe9a0
#define HDR_LSI_L2_S_EOTF_POSY3		0xe9a4
#define HDR_LSI_L2_S_EOTF_POSY4		0xe9a8
#define HDR_LSI_L2_S_EOTF_POSY5		0xe9ac
#define HDR_LSI_L2_S_EOTF_POSY6		0xe9b0
#define HDR_LSI_L2_S_EOTF_POSY7		0xe9b4
#define HDR_LSI_L2_S_EOTF_POSY8		0xe9b8
#define HDR_LSI_L2_S_EOTF_POSY9		0xe9bc
#define HDR_LSI_L2_S_EOTF_POSY10		0xe9c0
#define HDR_LSI_L2_S_EOTF_POSY11		0xe9c4
#define HDR_LSI_L2_S_EOTF_POSY12		0xe9c8
#define HDR_LSI_L2_S_EOTF_POSY13		0xe9cc
#define HDR_LSI_L2_S_EOTF_POSY14		0xe9d0
#define HDR_LSI_L2_S_EOTF_POSY15		0xe9d4
#define HDR_LSI_L2_S_EOTF_POSY16		0xe9d8
#define HDR_LSI_L2_S_EOTF_POSY17		0xe9dc
#define HDR_LSI_L2_S_EOTF_POSY18		0xe9e0
#define HDR_LSI_L2_S_EOTF_POSY19		0xe9e4
#define HDR_LSI_L2_S_EOTF_POSY20		0xe9e8
#define HDR_LSI_L2_S_EOTF_POSY21		0xe9ec
#define HDR_LSI_L2_S_EOTF_POSY22		0xe9f0
#define HDR_LSI_L2_S_EOTF_POSY23		0xe9f4
#define HDR_LSI_L2_S_EOTF_POSY24		0xe9f8
#define HDR_LSI_L2_S_EOTF_POSY25		0xe9fc
#define HDR_LSI_L2_S_EOTF_POSY26		0xea00
#define HDR_LSI_L2_S_EOTF_POSY27		0xea04
#define HDR_LSI_L2_S_EOTF_POSY28		0xea08
#define HDR_LSI_L2_S_EOTF_POSY29		0xea0c
#define HDR_LSI_L2_S_EOTF_POSY30		0xea10
#define HDR_LSI_L2_S_EOTF_POSY31		0xea14
#define HDR_LSI_L2_S_EOTF_POSY32		0xea18
#define HDR_LSI_L2_S_EOTF_POSY33		0xea1c
#define HDR_LSI_L2_S_EOTF_POSY34		0xea20
#define HDR_LSI_L2_S_EOTF_POSY35		0xea24
#define HDR_LSI_L2_S_EOTF_POSY36		0xea28
#define HDR_LSI_L2_S_EOTF_POSY37		0xea2c
#define HDR_LSI_L2_S_EOTF_POSY38		0xea30
#define HDR_LSI_L2_S_EOTF_POSY39		0xea34
#define HDR_LSI_L2_S_EOTF_POSY40		0xea38
#define HDR_LSI_L2_S_EOTF_POSY41		0xea3c
#define HDR_LSI_L2_S_EOTF_POSY42		0xea40
#define HDR_LSI_L2_S_EOTF_POSY43		0xea44
#define HDR_LSI_L2_S_EOTF_POSY44		0xea48
#define HDR_LSI_L2_S_EOTF_POSY45		0xea4c
#define HDR_LSI_L2_S_EOTF_POSY46		0xea50
#define HDR_LSI_L2_S_EOTF_POSY47		0xea54
#define HDR_LSI_L2_S_EOTF_POSY48		0xea58
#define HDR_LSI_L2_S_EOTF_POSY49		0xea5c
#define HDR_LSI_L2_S_EOTF_POSY50		0xea60
#define HDR_LSI_L2_S_EOTF_POSY51		0xea64
#define HDR_LSI_L2_S_EOTF_POSY52		0xea68
#define HDR_LSI_L2_S_EOTF_POSY53		0xea6c
#define HDR_LSI_L2_S_EOTF_POSY54		0xea70
#define HDR_LSI_L2_S_EOTF_POSY55		0xea74
#define HDR_LSI_L2_S_EOTF_POSY56		0xea78
#define HDR_LSI_L2_S_EOTF_POSY57		0xea7c
#define HDR_LSI_L2_S_EOTF_POSY58		0xea80
#define HDR_LSI_L2_S_EOTF_POSY59		0xea84
#define HDR_LSI_L2_S_EOTF_POSY60		0xea88
#define HDR_LSI_L2_S_EOTF_POSY61		0xea8c
#define HDR_LSI_L2_S_EOTF_POSY62		0xea90
#define HDR_LSI_L2_S_EOTF_POSY63		0xea94
#define HDR_LSI_L2_S_EOTF_POSY64		0xeb98
#define HDR_LSI_L2_S_GM_COEF0		0xeb9c
#define HDR_LSI_L2_S_GM_COEF1		0xeba0
#define HDR_LSI_L2_S_GM_COEF2		0xeba4
#define HDR_LSI_L2_S_GM_COEF3		0xeba8
#define HDR_LSI_L2_S_GM_COEF4		0xebac
#define HDR_LSI_L2_S_GM_COEF5		0xebb0
#define HDR_LSI_L2_S_GM_COEF6		0xebb4
#define HDR_LSI_L2_S_GM_COEF7		0xebb8
#define HDR_LSI_L2_S_GM_COEF8		0xebbc
#define HDR_LSI_L2_S_TS_GAIN		0xebc0

#define HDR_LSI_L3_OETF_POSX0			0xf00c
#define HDR_LSI_L3_OETF_POSX1			0xf010
#define HDR_LSI_L3_OETF_POSX2			0xf014
#define HDR_LSI_L3_OETF_POSX3			0xf018
#define HDR_LSI_L3_OETF_POSX4			0xf01c
#define HDR_LSI_L3_OETF_POSX5			0xf020
#define HDR_LSI_L3_OETF_POSX6			0xf024
#define HDR_LSI_L3_OETF_POSX7			0xf028
#define HDR_LSI_L3_OETF_POSX8			0xf02c
#define HDR_LSI_L3_OETF_POSX9			0xf030
#define HDR_LSI_L3_OETF_POSX10			0xf034
#define HDR_LSI_L3_OETF_POSX11			0xf038
#define HDR_LSI_L3_OETF_POSX12			0xf03c
#define HDR_LSI_L3_OETF_POSX13			0xf040
#define HDR_LSI_L3_OETF_POSX14			0xf044
#define HDR_LSI_L3_OETF_POSX15			0xf048
#define HDR_LSI_L3_OETF_POSX16			0xf04c
#define HDR_LSI_L3_OETF_POSY0			0xf050
#define HDR_LSI_L3_OETF_POSY1			0xf054
#define HDR_LSI_L3_OETF_POSY2			0xf058
#define HDR_LSI_L3_OETF_POSY3			0xf05c
#define HDR_LSI_L3_OETF_POSY4			0xf060
#define HDR_LSI_L3_OETF_POSY5			0xf064
#define HDR_LSI_L3_OETF_POSY6			0xf068
#define HDR_LSI_L3_OETF_POSY7			0xf06c
#define HDR_LSI_L3_OETF_POSY8			0xf070
#define HDR_LSI_L3_OETF_POSY9			0xf074
#define HDR_LSI_L3_OETF_POSY10			0xf078
#define HDR_LSI_L3_OETF_POSY11			0xf07c
#define HDR_LSI_L3_OETF_POSY12			0xf080
#define HDR_LSI_L3_OETF_POSY13			0xf084
#define HDR_LSI_L3_OETF_POSY14			0xf088
#define HDR_LSI_L3_OETF_POSY15			0xf08c
#define HDR_LSI_L3_OETF_POSY16			0xf090
#define HDR_LSI_L3_EOTF_POSX0			0xf094
#define HDR_LSI_L3_EOTF_POSX1			0xf098
#define HDR_LSI_L3_EOTF_POSX2			0xf09c
#define HDR_LSI_L3_EOTF_POSX3			0xf0a0
#define HDR_LSI_L3_EOTF_POSX4			0xf0a4
#define HDR_LSI_L3_EOTF_POSX5			0xf0a8
#define HDR_LSI_L3_EOTF_POSX6			0xf0ac
#define HDR_LSI_L3_EOTF_POSX7			0xf0b0
#define HDR_LSI_L3_EOTF_POSX8			0xf0b4
#define HDR_LSI_L3_EOTF_POSX9			0xf0b8
#define HDR_LSI_L3_EOTF_POSX10			0xf0bc
#define HDR_LSI_L3_EOTF_POSX11			0xf0c0
#define HDR_LSI_L3_EOTF_POSX12			0xf0c4
#define HDR_LSI_L3_EOTF_POSX13			0xf0c8
#define HDR_LSI_L3_EOTF_POSX14			0xf0cc
#define HDR_LSI_L3_EOTF_POSX15			0xf0d0
#define HDR_LSI_L3_EOTF_POSX16			0xf0d4
#define HDR_LSI_L3_EOTF_POSX17			0xf0d8
#define HDR_LSI_L3_EOTF_POSX18			0xf0dc
#define HDR_LSI_L3_EOTF_POSX19			0xf0e0
#define HDR_LSI_L3_EOTF_POSX20			0xf0e4
#define HDR_LSI_L3_EOTF_POSX21			0xf0e8
#define HDR_LSI_L3_EOTF_POSX22			0xf0ec
#define HDR_LSI_L3_EOTF_POSX23			0xf0f0
#define HDR_LSI_L3_EOTF_POSX24			0xf0f4
#define HDR_LSI_L3_EOTF_POSX25			0xf0f8
#define HDR_LSI_L3_EOTF_POSX26			0xf0fc
#define HDR_LSI_L3_EOTF_POSX27			0xf100
#define HDR_LSI_L3_EOTF_POSX28			0xf104
#define HDR_LSI_L3_EOTF_POSX29			0xf108
#define HDR_LSI_L3_EOTF_POSX30			0xf10c
#define HDR_LSI_L3_EOTF_POSX31			0xf110
#define HDR_LSI_L3_EOTF_POSX32			0xf114
#define HDR_LSI_L3_EOTF_POSX33			0xf118
#define HDR_LSI_L3_EOTF_POSX34			0xf11c
#define HDR_LSI_L3_EOTF_POSX35			0xf120
#define HDR_LSI_L3_EOTF_POSX36			0xf124
#define HDR_LSI_L3_EOTF_POSX37			0xf128
#define HDR_LSI_L3_EOTF_POSX38			0xf12c
#define HDR_LSI_L3_EOTF_POSX39			0xf130
#define HDR_LSI_L3_EOTF_POSX40			0xf134
#define HDR_LSI_L3_EOTF_POSX41			0xf138
#define HDR_LSI_L3_EOTF_POSX42			0xf13c
#define HDR_LSI_L3_EOTF_POSX43			0xf140
#define HDR_LSI_L3_EOTF_POSX44			0xf144
#define HDR_LSI_L3_EOTF_POSX45			0xf148
#define HDR_LSI_L3_EOTF_POSX46			0xf14c
#define HDR_LSI_L3_EOTF_POSX47			0xf150
#define HDR_LSI_L3_EOTF_POSX48			0xf154
#define HDR_LSI_L3_EOTF_POSX49			0xf158
#define HDR_LSI_L3_EOTF_POSX50			0xf15c
#define HDR_LSI_L3_EOTF_POSX51			0xf160
#define HDR_LSI_L3_EOTF_POSX52			0xf164
#define HDR_LSI_L3_EOTF_POSX53			0xf168
#define HDR_LSI_L3_EOTF_POSX54			0xf16c
#define HDR_LSI_L3_EOTF_POSX55			0xf170
#define HDR_LSI_L3_EOTF_POSX56			0xf174
#define HDR_LSI_L3_EOTF_POSX57			0xf178
#define HDR_LSI_L3_EOTF_POSX58			0xf17c
#define HDR_LSI_L3_EOTF_POSX59			0xf180
#define HDR_LSI_L3_EOTF_POSX60			0xf184
#define HDR_LSI_L3_EOTF_POSX61			0xf188
#define HDR_LSI_L3_EOTF_POSX62			0xf18c
#define HDR_LSI_L3_EOTF_POSX63			0xf190
#define HDR_LSI_L3_EOTF_POSX64			0xf194
#define HDR_LSI_L3_EOTF_POSY0			0xf198
#define HDR_LSI_L3_EOTF_POSY1			0xf19c
#define HDR_LSI_L3_EOTF_POSY2			0xf1a0
#define HDR_LSI_L3_EOTF_POSY3			0xf1a4
#define HDR_LSI_L3_EOTF_POSY4			0xf1a8
#define HDR_LSI_L3_EOTF_POSY5			0xf1ac
#define HDR_LSI_L3_EOTF_POSY6			0xf1b0
#define HDR_LSI_L3_EOTF_POSY7			0xf1b4
#define HDR_LSI_L3_EOTF_POSY8			0xf1b8
#define HDR_LSI_L3_EOTF_POSY9			0xf1bc
#define HDR_LSI_L3_EOTF_POSY10			0xf1c0
#define HDR_LSI_L3_EOTF_POSY11			0xf1c4
#define HDR_LSI_L3_EOTF_POSY12			0xf1c8
#define HDR_LSI_L3_EOTF_POSY13			0xf1cc
#define HDR_LSI_L3_EOTF_POSY14			0xf1d0
#define HDR_LSI_L3_EOTF_POSY15			0xf1d4
#define HDR_LSI_L3_EOTF_POSY16			0xf1d8
#define HDR_LSI_L3_EOTF_POSY17			0xf1dc
#define HDR_LSI_L3_EOTF_POSY18			0xf1e0
#define HDR_LSI_L3_EOTF_POSY19			0xf1e4
#define HDR_LSI_L3_EOTF_POSY20			0xf1e8
#define HDR_LSI_L3_EOTF_POSY21			0xf1ec
#define HDR_LSI_L3_EOTF_POSY22			0xf1f0
#define HDR_LSI_L3_EOTF_POSY23			0xf1f4
#define HDR_LSI_L3_EOTF_POSY24			0xf1f8
#define HDR_LSI_L3_EOTF_POSY25			0xf1fc
#define HDR_LSI_L3_EOTF_POSY26			0xf200
#define HDR_LSI_L3_EOTF_POSY27			0xf204
#define HDR_LSI_L3_EOTF_POSY28			0xf208
#define HDR_LSI_L3_EOTF_POSY29			0xf20c
#define HDR_LSI_L3_EOTF_POSY30			0xf210
#define HDR_LSI_L3_EOTF_POSY31			0xf214
#define HDR_LSI_L3_EOTF_POSY32			0xf218
#define HDR_LSI_L3_EOTF_POSY33			0xf21c
#define HDR_LSI_L3_EOTF_POSY34			0xf220
#define HDR_LSI_L3_EOTF_POSY35			0xf224
#define HDR_LSI_L3_EOTF_POSY36			0xf228
#define HDR_LSI_L3_EOTF_POSY37			0xf22c
#define HDR_LSI_L3_EOTF_POSY38			0xf230
#define HDR_LSI_L3_EOTF_POSY39			0xf234
#define HDR_LSI_L3_EOTF_POSY40			0xf238
#define HDR_LSI_L3_EOTF_POSY41			0xf23c
#define HDR_LSI_L3_EOTF_POSY42			0xf240
#define HDR_LSI_L3_EOTF_POSY43			0xf244
#define HDR_LSI_L3_EOTF_POSY44			0xf248
#define HDR_LSI_L3_EOTF_POSY45			0xf24c
#define HDR_LSI_L3_EOTF_POSY46			0xf250
#define HDR_LSI_L3_EOTF_POSY47			0xf254
#define HDR_LSI_L3_EOTF_POSY48			0xf258
#define HDR_LSI_L3_EOTF_POSY49			0xf25c
#define HDR_LSI_L3_EOTF_POSY50			0xf260
#define HDR_LSI_L3_EOTF_POSY51			0xf264
#define HDR_LSI_L3_EOTF_POSY52			0xf268
#define HDR_LSI_L3_EOTF_POSY53			0xf26c
#define HDR_LSI_L3_EOTF_POSY54			0xf270
#define HDR_LSI_L3_EOTF_POSY55			0xf274
#define HDR_LSI_L3_EOTF_POSY56			0xf278
#define HDR_LSI_L3_EOTF_POSY57			0xf27c
#define HDR_LSI_L3_EOTF_POSY58			0xf280
#define HDR_LSI_L3_EOTF_POSY59			0xf284
#define HDR_LSI_L3_EOTF_POSY60			0xf288
#define HDR_LSI_L3_EOTF_POSY61			0xf28c
#define HDR_LSI_L3_EOTF_POSY62			0xf290
#define HDR_LSI_L3_EOTF_POSY63			0xf294
#define HDR_LSI_L3_EOTF_POSY64			0xf298
#define HDR_LSI_L3_EOTF_POSY65			0xf29c
#define HDR_LSI_L3_EOTF_POSY66			0xf2a0
#define HDR_LSI_L3_EOTF_POSY67			0xf2a4
#define HDR_LSI_L3_EOTF_POSY68			0xf2a8
#define HDR_LSI_L3_EOTF_POSY69			0xf2ac
#define HDR_LSI_L3_EOTF_POSY70			0xf2b0
#define HDR_LSI_L3_EOTF_POSY71			0xf2b4
#define HDR_LSI_L3_EOTF_POSY72			0xf2b8
#define HDR_LSI_L3_EOTF_POSY73			0xf2bc
#define HDR_LSI_L3_EOTF_POSY74			0xf2c0
#define HDR_LSI_L3_EOTF_POSY75			0xf2c4
#define HDR_LSI_L3_EOTF_POSY76			0xf2c8
#define HDR_LSI_L3_EOTF_POSY77			0xf2cc
#define HDR_LSI_L3_EOTF_POSY78			0xf2d0
#define HDR_LSI_L3_EOTF_POSY79			0xf2d4
#define HDR_LSI_L3_EOTF_POSY80			0xf2d8
#define HDR_LSI_L3_EOTF_POSY81			0xf2dc
#define HDR_LSI_L3_EOTF_POSY82			0xf2e0
#define HDR_LSI_L3_EOTF_POSY83			0xf2e4
#define HDR_LSI_L3_EOTF_POSY84			0xf2e8
#define HDR_LSI_L3_EOTF_POSY85			0xf2ec
#define HDR_LSI_L3_EOTF_POSY86			0xf2f0
#define HDR_LSI_L3_EOTF_POSY87			0xf2f4
#define HDR_LSI_L3_EOTF_POSY88			0xf2f8
#define HDR_LSI_L3_EOTF_POSY89			0xf2fc
#define HDR_LSI_L3_EOTF_POSY90			0xf300
#define HDR_LSI_L3_EOTF_POSY91			0xf304
#define HDR_LSI_L3_EOTF_POSY92			0xf308
#define HDR_LSI_L3_EOTF_POSY93			0xf30c
#define HDR_LSI_L3_EOTF_POSY94			0xf310
#define HDR_LSI_L3_EOTF_POSY95			0xf314
#define HDR_LSI_L3_EOTF_POSY96			0xf318
#define HDR_LSI_L3_EOTF_POSY97			0xf31c
#define HDR_LSI_L3_EOTF_POSY98			0xf320
#define HDR_LSI_L3_EOTF_POSY99			0xf324
#define HDR_LSI_L3_EOTF_POSY100			0xf328
#define HDR_LSI_L3_EOTF_POSY101			0xf32c
#define HDR_LSI_L3_EOTF_POSY102			0xf330
#define HDR_LSI_L3_EOTF_POSY103			0xf334
#define HDR_LSI_L3_EOTF_POSY104			0xf338
#define HDR_LSI_L3_EOTF_POSY105			0xf33c
#define HDR_LSI_L3_EOTF_POSY106			0xf340
#define HDR_LSI_L3_EOTF_POSY107			0xf344
#define HDR_LSI_L3_EOTF_POSY108			0xf348
#define HDR_LSI_L3_EOTF_POSY109			0xf34c
#define HDR_LSI_L3_EOTF_POSY110			0xf350
#define HDR_LSI_L3_EOTF_POSY111			0xf354
#define HDR_LSI_L3_EOTF_POSY112			0xf358
#define HDR_LSI_L3_EOTF_POSY113			0xf35c
#define HDR_LSI_L3_EOTF_POSY114			0xf360
#define HDR_LSI_L3_EOTF_POSY115			0xf364
#define HDR_LSI_L3_EOTF_POSY116			0xf368
#define HDR_LSI_L3_EOTF_POSY117			0xf36c
#define HDR_LSI_L3_EOTF_POSY118			0xf370
#define HDR_LSI_L3_EOTF_POSY119			0xf374
#define HDR_LSI_L3_EOTF_POSY120			0xf378
#define HDR_LSI_L3_EOTF_POSY121			0xf37c
#define HDR_LSI_L3_EOTF_POSY122			0xf380
#define HDR_LSI_L3_EOTF_POSY123			0xf384
#define HDR_LSI_L3_EOTF_POSY124			0xf388
#define HDR_LSI_L3_EOTF_POSY125			0xf38c
#define HDR_LSI_L3_EOTF_POSY126			0xf390
#define HDR_LSI_L3_EOTF_POSY127			0xf394
#define HDR_LSI_L3_EOTF_POSY128			0xf398
#define HDR_LSI_L3_GM_COEF0			0xf39c
#define HDR_LSI_L3_GM_COEF1			0xf3a0
#define HDR_LSI_L3_GM_COEF2			0xf3a4
#define HDR_LSI_L3_GM_COEF3			0xf3a8
#define HDR_LSI_L3_GM_COEF4			0xf3ac
#define HDR_LSI_L3_GM_COEF5			0xf3b0
#define HDR_LSI_L3_GM_COEF6			0xf3b4
#define HDR_LSI_L3_GM_COEF7			0xf3b8
#define HDR_LSI_L3_GM_COEF8			0xf3bc
#define HDR_LSI_L3_AM_POSX0			0xf3c8
#define HDR_LSI_L3_AM_POSX1			0xf3cc
#define HDR_LSI_L3_AM_POSX2			0xf3d0
#define HDR_LSI_L3_AM_POSX3			0xf3d4
#define HDR_LSI_L3_AM_POSX4			0xf3d8
#define HDR_LSI_L3_AM_POSX5			0xf3dc
#define HDR_LSI_L3_AM_POSX6			0xf3e0
#define HDR_LSI_L3_AM_POSX7			0xf3e4
#define HDR_LSI_L3_AM_POSX8			0xf3e8
#define HDR_LSI_L3_AM_POSY0			0xf3ec
#define HDR_LSI_L3_AM_POSY1			0xf3f0
#define HDR_LSI_L3_AM_POSY2			0xf3f4
#define HDR_LSI_L3_AM_POSY3			0xf3f8
#define HDR_LSI_L3_AM_POSY4			0xf3fc
#define HDR_LSI_L3_AM_POSY5			0xf400
#define HDR_LSI_L3_AM_POSY6			0xf404
#define HDR_LSI_L3_AM_POSY7			0xf408
#define HDR_LSI_L3_AM_POSY8			0xf40c
#define HDR_LSI_L3_AM_POSY9			0xf410
#define HDR_LSI_L3_AM_POSY10			0xf414
#define HDR_LSI_L3_AM_POSY11			0xf418
#define HDR_LSI_L3_AM_POSY12			0xf41c
#define HDR_LSI_L3_AM_POSY13			0xf420
#define HDR_LSI_L3_AM_POSY14			0xf424
#define HDR_LSI_L3_AM_POSY15			0xf428
#define HDR_LSI_L3_AM_POSY16			0xf42c

#define HDR_LSI_L3_TM_POSX0			0xf44c
#define HDR_LSI_L3_TM_POSX1			0xf450
#define HDR_LSI_L3_TM_POSX2			0xf454
#define HDR_LSI_L3_TM_POSX3			0xf458
#define HDR_LSI_L3_TM_POSX4			0xf45c
#define HDR_LSI_L3_TM_POSX5			0xf460
#define HDR_LSI_L3_TM_POSX6			0xf464
#define HDR_LSI_L3_TM_POSX7			0xf468
#define HDR_LSI_L3_TM_POSX8			0xf46c
#define HDR_LSI_L3_TM_POSX9			0xf470
#define HDR_LSI_L3_TM_POSX10			0xf474
#define HDR_LSI_L3_TM_POSX11			0xf478
#define HDR_LSI_L3_TM_POSX12			0xf47c
#define HDR_LSI_L3_TM_POSX13			0xf480
#define HDR_LSI_L3_TM_POSX14			0xf484
#define HDR_LSI_L3_TM_POSX15			0xf488
#define HDR_LSI_L3_TM_POSX16			0xf48c
#define HDR_LSI_L3_TM_POSY0			0xf490
#define HDR_LSI_L3_TM_POSY1			0xf494
#define HDR_LSI_L3_TM_POSY2			0xf498
#define HDR_LSI_L3_TM_POSY3			0xf49c
#define HDR_LSI_L3_TM_POSY4			0xf4a0
#define HDR_LSI_L3_TM_POSY5			0xf4a4
#define HDR_LSI_L3_TM_POSY6			0xf4a8
#define HDR_LSI_L3_TM_POSY7			0xf4ac
#define HDR_LSI_L3_TM_POSY8			0xf4b0
#define HDR_LSI_L3_TM_POSY9			0xf4b4
#define HDR_LSI_L3_TM_POSY10			0xf4b8
#define HDR_LSI_L3_TM_POSY11			0xf4bc
#define HDR_LSI_L3_TM_POSY12			0xf4c0
#define HDR_LSI_L3_TM_POSY13			0xf4c4
#define HDR_LSI_L3_TM_POSY14			0xf4c8
#define HDR_LSI_L3_TM_POSY15			0xf4cc
#define HDR_LSI_L3_TM_POSY16			0xf4d0
#define HDR_LSI_L3_TM_POSY17			0xf4d4
#define HDR_LSI_L3_TM_POSY18			0xf4d8
#define HDR_LSI_L3_TM_POSY19			0xf4dc
#define HDR_LSI_L3_TM_POSY20			0xf4e0
#define HDR_LSI_L3_TM_POSY21			0xf4e4
#define HDR_LSI_L3_TM_POSY22			0xf4e8
#define HDR_LSI_L3_TM_POSY23			0xf4ec
#define HDR_LSI_L3_TM_POSY24			0xf4f0
#define HDR_LSI_L3_TM_POSY25			0xf4f4
#define HDR_LSI_L3_TM_POSY26			0xf4f8
#define HDR_LSI_L3_TM_POSY27			0xf4fc
#define HDR_LSI_L3_TM_POSY28			0xf500
#define HDR_LSI_L3_TM_POSY29			0xf504
#define HDR_LSI_L3_TM_POSY30			0xf508
#define HDR_LSI_L3_TM_POSY31			0xf50c
#define HDR_LSI_L3_TM_POSY32			0xf510

#define HDR_LSI_L3_S_OETF_POSX0			0xf80c
#define HDR_LSI_L3_S_OETF_POSX1			0xf810
#define HDR_LSI_L3_S_OETF_POSX2			0xf814
#define HDR_LSI_L3_S_OETF_POSX3			0xf818
#define HDR_LSI_L3_S_OETF_POSX4			0xf81c
#define HDR_LSI_L3_S_OETF_POSX5			0xf820
#define HDR_LSI_L3_S_OETF_POSX6			0xf824
#define HDR_LSI_L3_S_OETF_POSX7			0xf828
#define HDR_LSI_L3_S_OETF_POSX8			0xf82c
#define HDR_LSI_L3_S_OETF_POSX9			0xf830
#define HDR_LSI_L3_S_OETF_POSX10		0xf834
#define HDR_LSI_L3_S_OETF_POSX11		0xf838
#define HDR_LSI_L3_S_OETF_POSX12		0xf83c
#define HDR_LSI_L3_S_OETF_POSX13		0xf840
#define HDR_LSI_L3_S_OETF_POSX14		0xf844
#define HDR_LSI_L3_S_OETF_POSX15		0xf848
#define HDR_LSI_L3_S_OETF_POSX16		0xf84c
#define HDR_LSI_L3_S_OETF_POSY0			0xf850
#define HDR_LSI_L3_S_OETF_POSY1			0xf854
#define HDR_LSI_L3_S_OETF_POSY2			0xf858
#define HDR_LSI_L3_S_OETF_POSY3			0xf85c
#define HDR_LSI_L3_S_OETF_POSY4			0xf860
#define HDR_LSI_L3_S_OETF_POSY5			0xf864
#define HDR_LSI_L3_S_OETF_POSY6			0xf868
#define HDR_LSI_L3_S_OETF_POSY7			0xf86c
#define HDR_LSI_L3_S_OETF_POSY8			0xf870
#define HDR_LSI_L3_S_OETF_POSY9			0xf874
#define HDR_LSI_L3_S_OETF_POSY10		0xf878
#define HDR_LSI_L3_S_OETF_POSY11		0xf87c
#define HDR_LSI_L3_S_OETF_POSY12		0xf880
#define HDR_LSI_L3_S_OETF_POSY13		0xf884
#define HDR_LSI_L3_S_OETF_POSY14		0xf888
#define HDR_LSI_L3_S_OETF_POSY15		0xf88c
#define HDR_LSI_L3_S_OETF_POSY16		0xf890
#define HDR_LSI_L3_S_EOTF_POSX0			0xf894
#define HDR_LSI_L3_S_EOTF_POSX1			0xf898
#define HDR_LSI_L3_S_EOTF_POSX2			0xf89c
#define HDR_LSI_L3_S_EOTF_POSX3			0xf8a0
#define HDR_LSI_L3_S_EOTF_POSX4			0xf8a4
#define HDR_LSI_L3_S_EOTF_POSX5			0xf8a8
#define HDR_LSI_L3_S_EOTF_POSX6			0xf8ac
#define HDR_LSI_L3_S_EOTF_POSX7			0xf8b0
#define HDR_LSI_L3_S_EOTF_POSX8			0xf8b4
#define HDR_LSI_L3_S_EOTF_POSX9			0xf8b8
#define HDR_LSI_L3_S_EOTF_POSX10		0xf8bc
#define HDR_LSI_L3_S_EOTF_POSX11		0xf8c0
#define HDR_LSI_L3_S_EOTF_POSX12		0xf8c4
#define HDR_LSI_L3_S_EOTF_POSX13		0xf8c8
#define HDR_LSI_L3_S_EOTF_POSX14		0xf8cc
#define HDR_LSI_L3_S_EOTF_POSX15		0xf8d0
#define HDR_LSI_L3_S_EOTF_POSX16		0xf8d4
#define HDR_LSI_L3_S_EOTF_POSX17		0xf8d8
#define HDR_LSI_L3_S_EOTF_POSX18		0xf8dc
#define HDR_LSI_L3_S_EOTF_POSX19		0xf8e0
#define HDR_LSI_L3_S_EOTF_POSX20		0xf8e4
#define HDR_LSI_L3_S_EOTF_POSX21		0xf8e8
#define HDR_LSI_L3_S_EOTF_POSX22		0xf8ec
#define HDR_LSI_L3_S_EOTF_POSX23		0xf8f0
#define HDR_LSI_L3_S_EOTF_POSX24		0xf8f4
#define HDR_LSI_L3_S_EOTF_POSX25		0xf8f8
#define HDR_LSI_L3_S_EOTF_POSX26		0xf8fc
#define HDR_LSI_L3_S_EOTF_POSX27		0xf900
#define HDR_LSI_L3_S_EOTF_POSX28		0xf904
#define HDR_LSI_L3_S_EOTF_POSX29		0xf908
#define HDR_LSI_L3_S_EOTF_POSX30		0xf90c
#define HDR_LSI_L3_S_EOTF_POSX31		0xf910
#define HDR_LSI_L3_S_EOTF_POSX32		0xf994
#define HDR_LSI_L3_S_EOTF_POSY0			0xf998
#define HDR_LSI_L3_S_EOTF_POSY1			0xf99c
#define HDR_LSI_L3_S_EOTF_POSY2			0xf9a0
#define HDR_LSI_L3_S_EOTF_POSY3			0xf9a4
#define HDR_LSI_L3_S_EOTF_POSY4			0xf9a8
#define HDR_LSI_L3_S_EOTF_POSY5			0xf9ac
#define HDR_LSI_L3_S_EOTF_POSY6			0xf9b0
#define HDR_LSI_L3_S_EOTF_POSY7			0xf9b4
#define HDR_LSI_L3_S_EOTF_POSY8			0xf9b8
#define HDR_LSI_L3_S_EOTF_POSY9			0xf9bc
#define HDR_LSI_L3_S_EOTF_POSY10		0xf9c0
#define HDR_LSI_L3_S_EOTF_POSY11		0xf9c4
#define HDR_LSI_L3_S_EOTF_POSY12		0xf9c8
#define HDR_LSI_L3_S_EOTF_POSY13		0xf9cc
#define HDR_LSI_L3_S_EOTF_POSY14		0xf9d0
#define HDR_LSI_L3_S_EOTF_POSY15		0xf9d4
#define HDR_LSI_L3_S_EOTF_POSY16		0xf9d8
#define HDR_LSI_L3_S_EOTF_POSY17		0xf9dc
#define HDR_LSI_L3_S_EOTF_POSY18		0xf9e0
#define HDR_LSI_L3_S_EOTF_POSY19		0xf9e4
#define HDR_LSI_L3_S_EOTF_POSY20		0xf9e8
#define HDR_LSI_L3_S_EOTF_POSY21		0xf9ec
#define HDR_LSI_L3_S_EOTF_POSY22		0xf9f0
#define HDR_LSI_L3_S_EOTF_POSY23		0xf9f4
#define HDR_LSI_L3_S_EOTF_POSY24		0xf9f8
#define HDR_LSI_L3_S_EOTF_POSY25		0xf9fc
#define HDR_LSI_L3_S_EOTF_POSY26		0xfa00
#define HDR_LSI_L3_S_EOTF_POSY27		0xfa04
#define HDR_LSI_L3_S_EOTF_POSY28		0xfa08
#define HDR_LSI_L3_S_EOTF_POSY29		0xfa0c
#define HDR_LSI_L3_S_EOTF_POSY30		0xfa10
#define HDR_LSI_L3_S_EOTF_POSY31		0xfa14
#define HDR_LSI_L3_S_EOTF_POSY32		0xfa18
#define HDR_LSI_L3_S_EOTF_POSY33		0xfa1c
#define HDR_LSI_L3_S_EOTF_POSY34		0xfa20
#define HDR_LSI_L3_S_EOTF_POSY35		0xfa24
#define HDR_LSI_L3_S_EOTF_POSY36		0xfa28
#define HDR_LSI_L3_S_EOTF_POSY37		0xfa2c
#define HDR_LSI_L3_S_EOTF_POSY38		0xfa30
#define HDR_LSI_L3_S_EOTF_POSY39		0xfa34
#define HDR_LSI_L3_S_EOTF_POSY40		0xfa38
#define HDR_LSI_L3_S_EOTF_POSY41		0xfa3c
#define HDR_LSI_L3_S_EOTF_POSY42		0xfa40
#define HDR_LSI_L3_S_EOTF_POSY43		0xfa44
#define HDR_LSI_L3_S_EOTF_POSY44		0xfa48
#define HDR_LSI_L3_S_EOTF_POSY45		0xfa4c
#define HDR_LSI_L3_S_EOTF_POSY46		0xfa50
#define HDR_LSI_L3_S_EOTF_POSY47		0xfa54
#define HDR_LSI_L3_S_EOTF_POSY48		0xfa58
#define HDR_LSI_L3_S_EOTF_POSY49		0xfa5c
#define HDR_LSI_L3_S_EOTF_POSY50		0xfa60
#define HDR_LSI_L3_S_EOTF_POSY51		0xfa64
#define HDR_LSI_L3_S_EOTF_POSY52		0xfa68
#define HDR_LSI_L3_S_EOTF_POSY53		0xfa6c
#define HDR_LSI_L3_S_EOTF_POSY54		0xfa70
#define HDR_LSI_L3_S_EOTF_POSY55		0xfa74
#define HDR_LSI_L3_S_EOTF_POSY56		0xfa78
#define HDR_LSI_L3_S_EOTF_POSY57		0xfa7c
#define HDR_LSI_L3_S_EOTF_POSY58		0xfa80
#define HDR_LSI_L3_S_EOTF_POSY59		0xfa84
#define HDR_LSI_L3_S_EOTF_POSY60		0xfa88
#define HDR_LSI_L3_S_EOTF_POSY61		0xfa8c
#define HDR_LSI_L3_S_EOTF_POSY62		0xfa90
#define HDR_LSI_L3_S_EOTF_POSY63		0xfa94
#define HDR_LSI_L3_S_EOTF_POSY64		0xfb98
#define HDR_LSI_L3_S_GM_COEF0			0xfb9c
#define HDR_LSI_L3_S_GM_COEF1			0xfba0
#define HDR_LSI_L3_S_GM_COEF2			0xfba4
#define HDR_LSI_L3_S_GM_COEF3			0xfba8
#define HDR_LSI_L3_S_GM_COEF4			0xfbac
#define HDR_LSI_L3_S_GM_COEF5			0xfbb0
#define HDR_LSI_L3_S_GM_COEF6			0xfbb4
#define HDR_LSI_L3_S_GM_COEF7			0xfbb8
#define HDR_LSI_L3_S_GM_COEF8			0xfbbc
#define HDR_LSI_L3_S_AM_POSX0			0xfbc8
#define HDR_LSI_L3_S_AM_POSX1			0xfbcc
#define HDR_LSI_L3_S_AM_POSX2			0xfbd0
#define HDR_LSI_L3_S_AM_POSX3			0xfbd4
#define HDR_LSI_L3_S_AM_POSX4			0xfbd8
#define HDR_LSI_L3_S_AM_POSX5			0xfbdc
#define HDR_LSI_L3_S_AM_POSX6			0xfbe0
#define HDR_LSI_L3_S_AM_POSX7			0xfbe4
#define HDR_LSI_L3_S_AM_POSX8			0xfbe8
#define HDR_LSI_L3_S_AM_POSY0			0xfbec
#define HDR_LSI_L3_S_AM_POSY1			0xfbf0
#define HDR_LSI_L3_S_AM_POSY2			0xfbf4
#define HDR_LSI_L3_S_AM_POSY3			0xfbf8
#define HDR_LSI_L3_S_AM_POSY4			0xfbfc
#define HDR_LSI_L3_S_AM_POSY5			0xfc00
#define HDR_LSI_L3_S_AM_POSY6			0xfc04
#define HDR_LSI_L3_S_AM_POSY7			0xfc08
#define HDR_LSI_L3_S_AM_POSY8			0xfc0c
#define HDR_LSI_L3_S_AM_POSY9			0xfc10
#define HDR_LSI_L3_S_AM_POSY10			0xfc14
#define HDR_LSI_L3_S_AM_POSY11			0xfc18
#define HDR_LSI_L3_S_AM_POSY12			0xfc1c
#define HDR_LSI_L3_S_AM_POSY13			0xfc20
#define HDR_LSI_L3_S_AM_POSY14			0xfc24
#define HDR_LSI_L3_S_AM_POSY15			0xfc28
#define HDR_LSI_L3_S_AM_POSY16			0xfc2c
#define HDR_LSI_L3_S_TM_POSX0			0xfc4c
#define HDR_LSI_L3_S_TM_POSX1			0xfc50
#define HDR_LSI_L3_S_TM_POSX2			0xfc54
#define HDR_LSI_L3_S_TM_POSX3			0xfc58
#define HDR_LSI_L3_S_TM_POSX4			0xfc5c
#define HDR_LSI_L3_S_TM_POSX5			0xfc60
#define HDR_LSI_L3_S_TM_POSX6			0xfc64
#define HDR_LSI_L3_S_TM_POSX7			0xfc68
#define HDR_LSI_L3_S_TM_POSX8			0xfc6c
#define HDR_LSI_L3_S_TM_POSX9			0xfc70
#define HDR_LSI_L3_S_TM_POSX10			0xfc74
#define HDR_LSI_L3_S_TM_POSX11			0xfc78
#define HDR_LSI_L3_S_TM_POSX12			0xfc7c
#define HDR_LSI_L3_S_TM_POSX13			0xfc80
#define HDR_LSI_L3_S_TM_POSX14			0xfc84
#define HDR_LSI_L3_S_TM_POSX15			0xfc88
#define HDR_LSI_L3_S_TM_POSX16			0xfc8c
#define HDR_LSI_L3_S_TM_POSY0			0xfc90
#define HDR_LSI_L3_S_TM_POSY1			0xfc94
#define HDR_LSI_L3_S_TM_POSY2			0xfc98
#define HDR_LSI_L3_S_TM_POSY3			0xfc9c
#define HDR_LSI_L3_S_TM_POSY4			0xfca0
#define HDR_LSI_L3_S_TM_POSY5			0xfca4
#define HDR_LSI_L3_S_TM_POSY6			0xfca8
#define HDR_LSI_L3_S_TM_POSY7			0xfcac
#define HDR_LSI_L3_S_TM_POSY8			0xfcb0
#define HDR_LSI_L3_S_TM_POSY9			0xfcb4
#define HDR_LSI_L3_S_TM_POSY10			0xfcb8
#define HDR_LSI_L3_S_TM_POSY11			0xfcbc
#define HDR_LSI_L3_S_TM_POSY12			0xfcc0
#define HDR_LSI_L3_S_TM_POSY13			0xfcc4
#define HDR_LSI_L3_S_TM_POSY14			0xfcc8
#define HDR_LSI_L3_S_TM_POSY15			0xfccc
#define HDR_LSI_L3_S_TM_POSY16			0xfcd0
#define HDR_LSI_L3_S_TM_POSY17			0xfcd4
#define HDR_LSI_L3_S_TM_POSY18			0xfcd8
#define HDR_LSI_L3_S_TM_POSY19			0xfcdc
#define HDR_LSI_L3_S_TM_POSY20			0xfce0
#define HDR_LSI_L3_S_TM_POSY21			0xfce4
#define HDR_LSI_L3_S_TM_POSY22			0xfce8
#define HDR_LSI_L3_S_TM_POSY23			0xfcec
#define HDR_LSI_L3_S_TM_POSY24			0xfcf0
#define HDR_LSI_L3_S_TM_POSY25			0xfcf4
#define HDR_LSI_L3_S_TM_POSY26			0xfcf8
#define HDR_LSI_L3_S_TM_POSY27			0xfcfc
#define HDR_LSI_L3_S_TM_POSY28			0xfd00
#define HDR_LSI_L3_S_TM_POSY29			0xfd04
#define HDR_LSI_L3_S_TM_POSY30			0xfd08
#define HDR_LSI_L3_S_TM_POSY31			0xfd0c
#define HDR_LSI_L3_S_TM_POSY32			0xfd10


#endif /* DPP_REGS_H_ */
