/* drivers/video/fbdev/exynos/dpu30/cal_3830/regs-dpp.h
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
 * DPU_DMA SFR base address : 0x130B0000
 * - GLOBAL         	: 0x130B0000
 * - IDMA G0		: 0x130B1000
 * - IDMA G1		: 0x130B2000
 * - IDMA G2		: 0x130B3000
 * - IDMA VG0		: 0x130B4000
 */
#define DPU_DMA_VERSION				0x0000
#define DPU_DMA_VER				0x0002

#define DPU_DMA_QCH_EN				0x000C
#define DMA_QCH_EN				(1 << 0)

#define DPU_DMA_SWRST				0x0010
#define DMA_ALL_SWRST				(1 << 0)

#define DPU_DMA_GLB_CGEN_CH			0x0014
#define DMA_SFR_CGEN(_v)			((_v) << 16)
#define DMA_SFR_CGEN_MASK			(1 << 16)
#define DMA_INT_CGEN(_v)			((_v) << 0)
#define DMA_INT_CGEN_MASK			(0xFFF << 0)

#define DPU_DMA_TEST_PATTERN0_3			0x0020
#define DPU_DMA_TEST_PATTERN0_2			0x0024
#define DPU_DMA_TEST_PATTERN0_1			0x0028
#define DPU_DMA_TEST_PATTERN0_0			0x002C
#define DPU_DMA_TEST_PATTERN1_3			0x0030
#define DPU_DMA_TEST_PATTERN1_2			0x0034
#define DPU_DMA_TEST_PATTERN1_1			0x0038
#define DPU_DMA_TEST_PATTERN1_0			0x003C

/* _n: [0,7], _v: [0x0, 0xF] */

#define IDMA_QOS_LUT07_00			0x0070
#define IDMA_QOS_LUT15_08			0x0074
#define IDMA_QOS_LUT(_n, _v)			((_v) << (4*(_n)))
#define IDMA_QOS_LUT_MASK(_n)			(0xF << (4*(_n)))

#define IDMA_PERFORMANCE_CON			0x0078
#define IDMA_DEGRADATION_TIME(_v)		((_v) << 16)
#define IDMA_DEGRADATION_TIME_MASK		(0xFFFF << 16)
#define IDMA_IN_IC_MAX_DEG(_v)			((_v) << 4)
#define IDMA_IN_IC_MAX_DEG_MASK			(0x7F << 4)
#define IDMA_DEGRADATION_EN			(1 << 0)

#define IDMA_DEADLOCK_NUM			0x007C

#define DPU_DMA_DEBUG_CONTROL			0x0100
#define DPU_DMA_DEBUG_CONTROL_SEL(_v)		((_v) << 16)
#define DPU_DMA_DEBUG_CONTROL_EN		(0x1 << 0)

#define DPU_DMA_DEBUG_DATA			0x0104

/*
 * 1.1 - IDMA Register
 * < DMA.offset >
 *  G0      G1      G2      VG0
 *  0x1000  0x2000  0x3000  0x4000
 */
#define IDMA_ENABLE				0x0000
#define IDMA_SRESET				(1 << 24)
#define IDMA_SFR_CLOCK_GATE_EN			(1 << 10)
#define IDMA_SRAM_CLOCK_GATE_EN			(1 << 9)
#define IDMA_FRAME_START_FORCE			(1 << 5)
#define IDMA_SFR_UPDATE_FORCE			(1 << 4)
#define IDMA_OP_STATUS				(1 << 2)
#define OP_STATUS_IDLE				(0)
#define OP_STATUS_BUSY				(1)

#define IDMA_IRQ				0x0004
/* Not Supported IRQ */
#define IDMA_AXI_ADDR_ERR_IRQ                   (1 << 26)
#define IDMA_AFBC_CONFLICT_IRQ                  (1 << 25)
#define IDMA_SBWC_ERR_IRQ                       (1 << 23)
/* Supported IRQ */
#define IDMA_RECOVERY_TRG_IRQ			(1 << 22)
#define IDMA_CONFIG_ERROR			(1 << 21)
#define IDMA_LOCAL_HW_RESET_DONE		(1 << 20)
#define IDMA_READ_SLAVE_ERROR			(1 << 19)
#define IDMA_STATUS_DEADLOCK_IRQ		(1 << 17)
#define IDMA_STATUS_FRAMEDONE_IRQ		(1 << 16)
#define IDMA_ALL_IRQ_CLEAR			(0x7B << 16)
#define IDMA_RECOVERY_TRG_MASK			(1 << 7)
#define IDMA_CONFIG_ERROR_MASK			(1 << 6)
#define IDMA_LOCAL_HW_RESET_DONE_MASK		(1 << 5)
#define IDMA_READ_SLAVE_ERROR_MASK		(1 << 4)
#define IDMA_IRQ_DEADLOCK_MASK			(1 << 2)
#define IDMA_IRQ_FRAMEDONE_MASK			(1 << 1)
#define IDMA_ALL_IRQ_MASK			(0x7B << 1)
#define IDMA_IRQ_ENABLE				(1 << 0)

#define IDMA_IN_CON				0x0008
#define IDMA_IN_IC_MAX(_v)			((_v) << 19)
#define IDMA_IN_IC_MAX_MASK			(0x7F << 19)
#define IDMA_IMG_FORMAT(_v)			((_v) << 11)
#define IDMA_IMG_FORMAT_MASK			(0x1F << 11)
/* Supported IMG_FORMAT - RGB */
#define IDMA_IMG_FORMAT_ARGB8888		(0)
#define IDMA_IMG_FORMAT_ABGR8888		(1)
#define IDMA_IMG_FORMAT_RGBA8888		(2)
#define IDMA_IMG_FORMAT_BGRA8888		(3)
#define IDMA_IMG_FORMAT_XRGB8888		(4)
#define IDMA_IMG_FORMAT_XBGR8888		(5)
#define IDMA_IMG_FORMAT_RGBX8888		(6)
#define IDMA_IMG_FORMAT_BGRX8888		(7)
#define IDMA_IMG_FORMAT_RGB565			(8)
/* Not supported IMG_FORMAT - RGB */
#define IDMA_IMG_FORMAT_BGR565			(0xFF)
/* Supported IMG_FORMAT - RGB */
#define IDMA_IMG_FORMAT_ARGB1555		(12)
#define IDMA_IMG_FORMAT_ARGB4444		(13)
/* Not supported IMG_FORMAT - RGB */
#define IDMA_IMG_FORMAT_ABGR1555		(0xFF)
#define IDMA_IMG_FORMAT_ABGR4444		(0xFF)
#define IDMA_IMG_FORMAT_ARGB2101010		(0xFF)
#define IDMA_IMG_FORMAT_ABGR2101010		(0xFF)
#define IDMA_IMG_FORMAT_RGBA1010102		(0xFF)
#define IDMA_IMG_FORMAT_BGRA1010102		(0xFF)
#define IDMA_IMG_FORMAT_RGBA5551		(0xFF)
#define IDMA_IMG_FORMAT_RGBA4444		(0xFF)
#define IDMA_IMG_FORMAT_BGRA5551		(0xFF)
#define IDMA_IMG_FORMAT_BGRA4444		(0xFF)
/* Supported IMG_FORMAT - YUV */
#define IDMA_IMG_FORMAT_YUV420_2P		(24)
#define IDMA_IMG_FORMAT_YVU420_2P		(25)
/* Not supported IMG_FORMAT - YUV */
#define IDMA_IMG_FORMAT_YUV420_8P2		(0xFF)
#define IDMA_IMG_FORMAT_YVU420_8P2		(0xFF)
#define IDMA_IMG_FORMAT_YUV420_P010		(0xFF)
#define IDMA_IMG_FORMAT_YVU420_P010		(0xFF)
#define IDMA_IMG_FORMAT_YVU422_2P		(0xFF)
#define IDMA_IMG_FORMAT_YUV422_2P		(0xFF)
#define IDMA_IMG_FORMAT_YVU422_8P2		(0xFF)
#define IDMA_IMG_FORMAT_YUV422_8P2		(0xFF)
#define IDMA_IMG_FORMAT_YVU422_P210		(0xFF)
#define IDMA_IMG_FORMAT_YUV422_P210		(0xFF)
#define IDMA_IMG_FORMAT_MAX			(26)
#define IDMA_IN_FLIP(_v)			((_v) << 8)
#define IDMA_IN_FLIP_MASK			(0x3 << 8)
#define IDMA_IN_CHROMINANCE_STRIDE_SEL		(1 << 4)
#define IDMA_BLOCK_EN				(1 << 3)

#define IDMA_OUT_CON				0x000C
#define IDMA_OUT_FRAME_ALPHA(_v)		((_v) << 24)
#define IDMA_OUT_FRAME_ALPHA_MASK		(0xFF << 24)

#define IDMA_SRC_SIZE				0x0010
#define IDMA_SRC_HEIGHT(_v)			((_v) << 16)
#define IDMA_SRC_HEIGHT_MASK			(0x3FFF << 16)
#define IDMA_SRC_WIDTH(_v)			((_v) << 0)
#define IDMA_SRC_WIDTH_MASK			(0xFFFF << 0)

#define IDMA_SRC_OFFSET				0x0014
#define IDMA_SRC_OFFSET_Y(_v)			((_v) << 16)
#define IDMA_SRC_OFFSET_Y_MASK			(0x1FFF << 16)
#define IDMA_SRC_OFFSET_X(_v)			((_v) << 0)
#define IDMA_SRC_OFFSET_X_MASK			(0x1FFF << 0)

#define IDMA_IMG_SIZE				0x0018
#define IDMA_IMG_HEIGHT(_v)			((_v) << 16)
#define IDMA_IMG_HEIGHT_MASK			(0x1FFF << 16)
#define IDMA_IMG_WIDTH(_v)			((_v) << 0)
#define IDMA_IMG_WIDTH_MASK			(0x1FFF << 0)

#define IDMA_CHROMINANCE_STRIDE			0x0020
#define IDMA_CHROM_STRIDE(_v)			((_v) << 0)
#define IDMA_CHROM_STRIDE_MASK			(0xFFFF << 0)

#define IDMA_BLOCK_OFFSET			0x0024
#define IDMA_BLK_OFFSET_Y(_v)			((_v) << 16)
#define IDMA_BLK_OFFSET_Y_MASK			(0x1FFF << 16)
#define IDMA_BLK_OFFSET_X(_v)			((_v) << 0)
#define IDMA_BLK_OFFSET_X_MASK			(0x1FFF << 0)

#define IDMA_BLOCK_SIZE				0x0028
#define IDMA_BLK_HEIGHT(_v)			((_v) << 16)
#define IDMA_BLK_HEIGHT_MASK			(0x1FFF << 16)
#define IDMA_BLK_WIDTH(_v)			((_v) << 0)
#define IDMA_BLK_WIDTH_MASK			(0x1FFF << 0)

#define IDMA_IN_BASE_ADDR_Y			0x0040
#define IDMA_IN_BASE_ADDR_C			0x0044

#define IDMA_IN_REQ_DEST			0x0048
#define IDMA_IN_REG_DEST_SEL(_v)		((_v) << 0)
#define IDMA_IN_REG_DEST_SEL_MASK		(0x3 << 0)

#define IDMA_DEADLOCK_EN			0x0050

#define IDMA_RECOVERY_CTRL			0x005C

#define IDMA_BUS_CON				0x0054
#define IDMA_BUS_ARCACHE			(0xF << 0)

#define IDMA_DYNAMIC_GATING_EN			0x0058
#define IDMA_DG_EN(_v)				((_v) << 0)
#define IDMA_DG_EN_MASK				(0xFF << 0)

#define IDMA_DEBUG_CONTROL			0x0060
#define IDMA_DEBUG_CONTROL_SEL(_v)		((_v) << 16)
#define IDMA_DEBUG_CONTROL_SEL_MASK		(0xFFFF << 16)
#define IDMA_DEBUG_CONTROL_EN			(0x1 << 0)

#define IDMA_DEBUG_DATA				0x0064

#define IDMA_CFG_ERR_STATE			0x0870
#define IDMA_CFG_ERR_SRC_WIDTH			(1 << 10)
#define IDMA_CFG_ERR_CHROM_STRIDE		(1 << 9)
#define IDMA_CFG_ERR_BASE_ADDR_Y		(1 << 8)
#define IDMA_CFG_ERR_BASE_ADDR_C		(1 << 7)
#define IDMA_CFG_ERR_IMG_WIDTH			(1 << 5)
#define IDMA_CFG_ERR_IMG_HEIGHT_ROTATION	(1 << 4)
#define IDMA_CFG_ERR_IMG_HEIGHT			(1 << 3)
#define IDMA_CFG_ERR_BLOCKING			(1 << 2)
#define IDMA_CFG_ERR_SRC_OFFSET_Y		(1 << 1)
#define IDMA_CFG_ERR_SRC_OFFSET_X		(1 << 0)
#define IDMA_CFG_ERR_GET(_v)			(((_v) >> 0) & 0x3FF)

#define ODMA_IRQ				0x0004
#define ODMA_CONFIG_ERROR			(1 << 28)
#define ODMA_LOCAL_HW_RESET_DONE		(1 << 20)
#define ODMA_WRITE_SLAVE_ERROR			(1 << 19)
#define ODMA_STATUS_DEADLOCK_IRQ		(1 << 17)
#define ODMA_STATUS_FRAMEDONE_IRQ		(1 << 16)
#define ODMA_ALL_IRQ_CLEAR			(0x101B << 16)

#define DMA_SHD_OFFSET				0x800

/*
 * DPP SFR base address : 0x13050000
 * - DPP VG0   		: 0x13054000
 */
#define DPP_ENABLE				0x0000
#define DPP_SRSET				(1 << 24)
#define DPP_SFR_CLOCK_GATE_EN			(1 << 10)
#define DPP_SRAM_CLOCK_GATE_EN			(1 << 9)
#define DPP_INT_CLOCK_GATE_EN			(1 << 8)
#define DPP_ALL_CLOCK_GATE_EN_MASK		(0x7 << 8)
#define DPP_SFR_UPDATE_FORCE			(1 << 4)
#define DPP_QCHANNEL_EN				(1 << 3)
#define DPP_OP_STATUS				(1 << 2)

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
#define DPP_CSC_TYPE_MASK			(1 << 18)
#define DPP_CSC_RANGE(_v)			((_v) << 17)
#define DPP_CSC_RANGE_MASK			(1 << 17)
#define DPP_CSC_MODE(_v)			((_v) << 16)
#define DPP_CSC_MODE_MASK			(1 << 16)
#define DPP_IMG_FORMAT(_v)			((_v) << 11)
#define DPP_IMG_FORMAT_MASK			(1 << 11)
/* Supported IMG_FORMAT - RGB */
#define DPP_IMG_FORMAT_ARGB8888			(0)
/* Not supported IMG_FORMAT - RGB */
#define DPP_IMG_FORMAT_ARGB8101010		(0xFF)
/* Supported IMG_FORMAT - YUV */
#define DPP_IMG_FORMAT_YUV420_8P		(1)
/* Not supported IMG_FORMAT - YUV */
#define DPP_IMG_FORMAT_YUV420_P010		(0xFF)
#define DPP_IMG_FORMAT_YUV420_8P2		(0xFF)
#define DPP_IMG_FORMAT_YUV422_8P		(0xFF)
#define DPP_IMG_FORMAT_YUV422_P210		(0xFF)
#define DPP_IMG_FORMAT_YUV422_8P2		(0xFF)

#define DPP_IMG_FORMAT_MAX 			(2)

#define DPP_IMG_SIZE				0x0018
#define DPP_IMG_HEIGHT(_v)			((_v) << 16)
#define DPP_IMG_HEIGHT_MASK			(0x7FF << 16)
#define DPP_IMG_WIDTH(_v)			((_v) << 0)
#define DPP_IMG_WIDTH_MASK			(0x7FF << 0)

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
#define DPP_CSC_COEF_H_MASK			(0xFFF << 16)
#define DPP_CSC_COEF_L(_v)			((_v) << 0)
#define DPP_CSC_COEF_L_MASK			(0xFFF << 0)
#define DPP_CSC_COEF_XX(_n, _v)			((_v) << (0 + (16 * (_n))))
#define DPP_CSC_COEF_XX_MASK(_n)		(0xFFF << (0 + (16 * (_n))))

#define DPP_DYNAMIC_GATING_EN			0x0A54
/* _n: [0 ~ 2], v: [0,1] */
#define DPP_DG_EN(_n, _v)			((_v) << (_n))
#define DPP_DG_EN_MASK(_n)			(1 << (_n))
#define DPP_DG_EN_ALL				(0x7 << 0)

#define DPP_DEBUG_CONTROL                       0x0C00
#define DPP_DEBUG_SEL                           0x0C04
#define DPP_DEBUG_DATA                          0x0C10

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
#define DPP_CFG_ERR_ODD_SIZE			(1 << 2)
#define DPP_CFG_ERR_MAX_SIZE			(1 << 1)
#define DPP_CFG_ERR_MIN_SIZE			(1 << 0)
#define DPP_CFG_ERR_GET(_v)			(((_v) >> 0) & 0x7)

#endif
