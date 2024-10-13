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


typedef enum {
	ROT_ONE_BYTE = 0,
	ROT_TWO_BYTES,
	ROT_BYTE_MAX
} ROT_PIXEL_FORMAT_E;

typedef enum {
	ROT_NORMAL = 0,
	ROT_UV422,
	ROT_DATA_FORMAT_MAX
} ROT_UV_MODE_E;

#define ROT_BASE DCAM_BASE
#define REG_ROTATION_CTRL (ROT_BASE + 0x0004UL)
#define REG_ROTATION_INT_STS (ROT_BASE + 0x003CUL)
#define REG_ROTATION_INT_MASK (ROT_BASE + 0x0040UL)
#define REG_ROTATION_INT_CLR (ROT_BASE + 0x0044UL)
#define REG_ROTATION_INT_RAW (ROT_BASE + 0x0048UL)
#define REG_ROTATION_ENDIAN_SEL (ROT_BASE + 0x006CUL)
#define REG_ROTATION_SRC_ADDR (ROT_BASE + 0x0080UL)
#define REG_ROTATION_DST_ADDR (ROT_BASE + 0x0084UL)
#define REG_ROTATION_PATH_CFG (ROT_BASE + 0x8000UL)
#define REG_ROTATION_ORIGWIDTH (ROT_BASE + 0x8004UL)
#define REG_ROTATION_OFFSET_START (ROT_BASE + 0x8008UL)
#define REG_ROTATION_IMG_SIZE (ROT_BASE + 0x800CUL)
#define REG_ROTATION_AHB_RESET (SPRD_MMAHB_BASE + 0x04UL)

#define ROT_IRQ_BIT (1 << 15)
#define ROT_START_BIT (1 << 5)
#define ROT_AXI_RD_WORD_ENDIAN_BIT (1 << 19)
#define ROT_AXI_WR_WORD_ENDIAN_BIT (1 << 18)
#define ROT_RD_ENDIAN_MASK (3 << 16)
#define ROT_WR_ENDIAN_MASK (3 << 14)

#define ROT_EB_BIT (1 << 0)
#define ROT_PIXEL_FORMAT_BIT (1 << 1)
#define ROT_MODE_MASK (3 << 2)
#define ROT_UV_MODE_BIT (1 << 4)

#define ROT_AHB_RESET_BIT (1 << 10)

#ifdef __cplusplus
}
#endif

#endif
