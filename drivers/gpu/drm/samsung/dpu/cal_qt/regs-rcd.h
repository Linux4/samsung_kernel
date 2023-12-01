/* SPDX-License-Identifier: GPL-2.0-only
 *
 * dpu30/cal_8825/regs-rcd.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Register definition file for Samsung Display Pre-Processor driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef RCD_REGS_H_
#define RCD_REGS_H_

enum rcd_regs_id {
	REGS_RCD0_ID = 0,
	REGS_RCD_ID_MAX
};

#define DMA_RCD_SHD_OFFSET			0x0400

#define DMA_RCD_ENABLE				0x0000
#define DMA_RCD_SRESET				(1 << 8)
#define DMA_RCD_SFR_UPDATE_FORCE			(1 << 4)
#define DMA_RCD_OP_STATUS				(1 << 2)
#define OP_STATUS_IDLE				(0)
#define OP_STATUS_BUSY				(1)
#define DMA_RCD_INST_OFF_PEND			(1 << 1)
#define INST_OFF_PEND				(1)
#define INST_OFF_NOT_PEND			(0)

#define DMA_RCD_IRQ				0x0004
#define DMA_RCD_RECOVERY_TRG_IRQ			(1 << 22)
#define DMA_RCD_CONFIG_ERR_IRQ			(1 << 21)
#define DMA_RCD_INST_OFF_DONE			(1 << 20)
#define DMA_RCD_READ_SLAVE_ERR_IRQ			(1 << 19)
#define DMA_RCD_DEADLOCK_IRQ			(1 << 17)
#define DMA_RCD_ALL_IRQ_CLEAR			(0xFFFF << 16)
#define DMA_RCD_RECOVERY_TRG_MASK			(1 << 7)
#define DMA_RCD_CONFIG_ERR_MASK			(1 << 6)
#define DMA_RCD_INST_OFF_DONE_MASK			(1 << 5)
#define DMA_RCD_READ_SLAVE_ERR_MASK		(1 << 4)
#define DMA_RCD_DEADLOCK_MASK			(1 << 2)
#define DMA_RCD_FRAME_DONE_MASK			(1 << 1)
#define DMA_RCD_ALL_IRQ_MASK			0xF6
#define DMA_RCD_IRQ_ENABLE				(1 << 0)

#define DMA_RCD_IN_CTRL_0				0x0008
#define DMA_RCD_IC_MAX(_v)				((_v) << 16)
#define DMA_RCD_IC_MAX_MASK			(0xff << 16)
#define DMA_RCD_BLOCK_EN				(1 << 0)

#define DMA_RCD_SRC_SIZE				0x0010
#define DMA_RCD_SRC_HEIGHT(_v)				((_v) << 16)
#define DMA_RCD_SRC_WIDTH_MASK				(0xFFFF << 16)
#define DMA_RCD_SRC_WIDTH(_v)				((_v) << 0)
#define DMA_RCD_SRC_HEIGHT_MASK				(0xFFFF << 0)

#define DMA_RCD_SRC_OFFSET				0x0014
#define DMA_RCD_SRC_OFFSET_Y(_v)			((_v) << 16)
#define DMA_RCD_SRC_OFFSET_Y_MASK			(0xFFFF << 16)
#define DMA_RCD_SRC_OFFSET_X(_v)			((_v) << 0)
#define DMA_RCD_SRC_OFFSET_X_MASK			(0xFFFF << 0)

#define DMA_RCD_IMG_SIZE				0x0018
#define DMA_RCD_IMG_HEIGHT(_v)			((_v) << 16)
#define DMA_RCD_IMG_HEIGHT_MASK			(0x3FFF << 16)
#define DMA_RCD_IMG_WIDTH(_v)			((_v) << 0)
#define DMA_RCD_IMG_WIDTH_MASK			(0x3FFF << 0)

#define DMA_RCD_BLOCK_OFFSET			0x0020
#define DMA_RCD_BLK_OFFSET_Y(_v)			((_v) << 16)
#define DMA_RCD_BLK_OFFSET_Y_MASK			(0x3FFF << 16)
#define DMA_RCD_BLK_OFFSET_X(_v)			((_v) << 0)
#define DMA_RCD_BLK_OFFSET_X_MASK			(0x3FFF << 0)

#define DMA_RCD_BLOCK_SIZE				0x0024
#define DMA_RCD_BLK_HEIGHT(_v)			((_v) << 16)
#define DMA_RCD_BLK_HEIGHT_MASK			(0x3FFF << 16)
#define DMA_RCD_BLK_WIDTH(_v)			((_v) << 0)
#define DMA_RCD_BLK_WIDTH_MASK			(0x3FFF << 0)

#define DMA_RCD_BLK_VALUE			0x0028
#define DMA_RCD_PIX_VALUE(_v)		((_v) << 24)
#define DMA_RCD_PIX_VALUE_MASK		(0xFF << 24)

#define DMA_RCD_BASEADDR_P0			0x0040

#define DMA_RCD_RECOVERY_CTRL			0x0070
#define DMA_RCD_RECOVERY_NUM(_v)			((_v) << 1)
#define DMA_RCD_RECOVERY_NUM_MASK			(0x7fffffff << 1)
#define DMA_RCD_RECOVERY_EN			(1 << 0)

#define DMA_RCD_DEADLOCK_CTRL			0x0100
#define DMA_RCD_DEADLOCK_NUM(_v)			((_v) << 1)
#define DMA_RCD_DEADLOCK_NUM_MASK			(0x7fffffff << 1)
#define DMA_RCD_DEADLOCK_NUM_EN			(1 << 0)

#define DMA_RCD_BUS_CTRL				0x0110
#define DMA_RCD_ARCACHE_P3(_v)			((_v) << 12)
#define DMA_RCD_ARCACHE_P3_MASK			(0xf << 12)
#define DMA_RCD_ARCACHE_P2(_v)			((_v) << 8)
#define DMA_RCD_ARCACHE_P2_MASK			(0xf << 8)
#define DMA_RCD_ARCACHE_P1(_v)			((_v) << 4)
#define DMA_RCD_ARCACHE_P1_MASK			(0xf << 4)
#define DMA_RCD_ARCACHE_P0(_v)			((_v) << 0)
#define DMA_RCD_ARCACHE_P0_MASK			(0xf << 0)

#define DMA_RCD_LLC_CTRL				0x0114
#define DMA_RCD_DATA_SAHRE_TYPE_P3(_v)		((_v) << 28)
#define DMA_RCD_DATA_SAHRE_TYPE_P3_MASK		(0x3 << 28)
#define DMA_RCD_LLC_HINT_P3(_v)			((_v) << 24)
#define DMA_RCD_LLC_HINT_P3_MASK			(0x7 << 24)
#define DMA_RCD_DATA_SAHRE_TYPE_P2(_v)		((_v) << 20)
#define DMA_RCD_DATA_SAHRE_TYPE_P2_MASK		(0x3 << 20)
#define DMA_RCD_LLC_HINT_P2(_v)			((_v) << 16)
#define DMA_RCD_LLC_HINT_P2_MASK			(0x7 << 16)
#define DMA_RCD_DATA_SAHRE_TYPE_P1(_v)		((_v) << 12)
#define DMA_RCD_DATA_SAHRE_TYPE_P1_MASK		(0x3 << 12)
#define DMA_RCD_LLC_HINT_P1(_v)			((_v) << 8)
#define DMA_RCD_LLC_HINT_P1_MASK			(0x7 << 8)
#define DMA_RCD_DATA_SAHRE_TYPE_P0(_v)		((_v) << 4)
#define DMA_RCD_DATA_SAHRE_TYPE_P0_MASK		(0x3 << 4)
#define DMA_RCD_LLC_HINT_P0(_v)			((_v) << 0)
#define DMA_RCD_LLC_HINT_P0_MASK			(0x7 << 0)

#define DMA_RCD_PERF_CTRL				0x0120
#define DMA_RCD_DEGRADATION_TIME(_v)		((_v) << 16)
#define DMA_RCD_DEGRADATION_TIME_MASK		(0xFFFF << 16)
#define DMA_RCD_IC_MAX_DEG(_v)			((_v) << 4)
#define DMA_RCD_IC_MAX_DEG_MASK			(0xFF << 4)
#define DMA_RCD_DEGRADATION_EN			(1 << 0)

/* _n: [0,7], _v: [0x0, 0xF] */
#define DMA_RCD_QOS_LUT_LOW			0x0130
#define DMA_RCD_QOS_LUT_HIGH			0x0134
#define DMA_RCD_QOS_LUT(_n, _v)			((_v) << (4*(_n)))
#define DMA_RCD_QOS_LUT_MASK(_n)			(0xF << (4*(_n)))

#define DMA_RCD_DYNAMIC_GATING_EN			0x0140
#define DMA_RCD_SRAM_CG_EN				(1 << 31)
#define DMA_RCD_DG_EN(_n, _v)			((_v) << (_n))
#define DMA_RCD_DG_EN_MASK(_n)			(1 << (_n))
#define DMA_RCD_DG_EN_ALL				(0x7FFFFFFF << 0)

#define DMA_RCD_MST_SECURITY			0x0200
#define DMA_RCD_IN_REQ_DEST			0x0308
#define DMA_RCD_REQ_DEST_SEL(_v)			((_v) << 0)
#define DMA_RCD_REQ_DEST_SEL_MASK			(0x3 << 0)

#endif
