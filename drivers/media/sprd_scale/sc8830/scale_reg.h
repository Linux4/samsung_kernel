/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _REG_SCALE_TIGER_H_
#define _REG_SCALE_TIGER_H_

#include <mach/globalregs.h>

#ifdef __cplusplus
extern "C"
{
#endif


#define SCALE_BASE DCAM_BASE
#define SCALE_CTRL (SCALE_BASE + 0x0004UL)
#define SCALE_CFG (SCALE_BASE + 0x0024UL)
#define SCALE_SRC_SIZE (SCALE_BASE + 0x0028UL)
#define SCALE_DST_SIZE (SCALE_BASE + 0x002CUL)
#define SCALE_TRIM_START (SCALE_BASE + 0x0030UL)
#define SCALE_TRIM_SIZE (SCALE_BASE + 0x0034UL)
#define SCALE_INT_STS (SCALE_BASE + 0x003CUL)
#define SCALE_INT_MASK (SCALE_BASE + 0x0040UL)
#define SCALE_INT_CLR (SCALE_BASE + 0x0044UL)
#define SCALE_INT_RAW (SCALE_BASE + 0x0048UL)
#define SCALE_FRM_OUT_Y (SCALE_BASE + 0x005CUL)
#define SCALE_FRM_OUT_U (SCALE_BASE + 0x0060UL)
#define SCALE_FRM_OUT_V (SCALE_BASE + 0x0064UL)
#define SCALE_ENDIAN_SEL (SCALE_BASE + 0x006CUL)
#define SCALE_FRM_IN_Y (SCALE_BASE + 0x0074UL)
#define SCALE_FRM_IN_U (SCALE_BASE + 0x0078UL)
#define SCALE_FRM_IN_V (SCALE_BASE + 0x007CUL)
#define SCALE_REV_BURST_IN_CFG (SCALE_BASE + 0x0098UL)
#define SCALE_REV_BURST_IN_TRIM_START (SCALE_BASE + 0x009CUL)
#define SCALE_REV_BURST_IN_TRIM_SIZE (SCALE_BASE + 0x00A0UL)
#define SCALE_REV_SLICE_CFG (SCALE_BASE + 0x00A4UL)
#define SCALE_REV_SLICE_O_VCNT (SCALE_BASE + 0x00ACUL)
#define SCALE_REG_START (SCALE_BASE + 0x0000UL)
#define SCALE_REG_END (SCALE_BASE + 0x0100UL)

#define SCALE_FRAME_WIDTH_MAX 8192
#define SCALE_FRAME_HEIGHT_MAX 8192
#define SCALE_SC_COEFF_MAX 4
#define SCALE_DECI_FAC_MAX 4
#define SCALE_LINE_BUF_LENGTH 4096

#define SCALE_IRQ_SLICE_BIT (1 << 17)
#define SCALE_IRQ_BIT (1 << 8)
#define SCALE_PATH_MASK (3 << 13)
#define SCALE_PATH_SELECT (2 << 13)
#define SCALE_PATH_EB_BIT (1 << 2)
#define SCALE_FRC_COPY_BIT (1 << 12)
#define SCALE_COEFF_FRC_COPY_BIT (1 << 16)
#if defined(CONFIG_ARCH_SCX30G)
#define SCALE_START_BIT (1 << 18)
#else
#define SCALE_START_BIT (1 << 4)
#endif
#define SCALE_IS_LAST_SLICE_BIT (1 << 14)
#define SCALE_INPUT_SLICE_HEIGHT_MASK (0x1FFF)
#define SCALE_OUTPUT_SLICE_HEIGHT_MASK (0x1FFF)
#define SCALE_BURST_SUB_EB_BIT (1 << 15)
#define SCALE_BURST_SUB_SAMPLE_MASK (3 << 13)
#define SCALE_BURST_SRC_WIDTH_MASK (0x1FFF)
#define SCALE_BURST_INPUT_MODE_MASK (3 << 16)
#define SCALE_AXI_RD_ENDIAN_BIT (1 << 19)
#define SCALE_AXI_WR_ENDIAN_BIT (1 << 18)
#define SCALE_INPUT_Y_ENDIAN_MASK (3 << 0)
#define SCALE_INPUT_UV_ENDIAN_MASK (3 << 2)
#define SCALE_OUTPUT_Y_ENDIAN_MASK (3 << 10)
#define SCALE_OUTPUT_UV_ENDIAN_MASK (3 << 12)
#define SCALE_OUTPUT_MODE_MASK (3 << 6)
#define SCALE_MODE_MASK (3 << 21)
#define SCALE_MODE_NORMAL_TYPE (0 << 21)
#define SCALE_MODE_SLICE_TYPE (1 << 21)
#define SCALE_SLICE_TYPE_BIT (1 << 13)
#define SCALE_BYPASS_BIT (1 << 20)
#define SCALE_DEC_X_MASK (3 << 0)
#define SCALE_DEC_X_EB_BIT (1 << 2)
#define SCALE_DEC_Y_MASK (3 << 3)
#define SCALE_DEC_Y_EB_BIT (1 << 5)

#ifdef __cplusplus
}
#endif

#endif
