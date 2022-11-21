/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
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
#ifndef _VPP_DITHER_REG_H_
#define _VPP_DITHER_REG_H_

extern struct vpp_dither_device *vpp_dither_ctx;

#define VPP_REG_BASE	(vpp_dither_ctx->reg_base_addr)

#define VPP_CFG			(VPP_REG_BASE + 0x0000)
#define VPP_INT_STS		(VPP_REG_BASE + 0x0010)
#define VPP_INT_MSK		(VPP_REG_BASE + 0x0014)
#define VPP_INT_CLR		(VPP_REG_BASE + 0x0018)
#define VPP_INT_RAW		(VPP_REG_BASE + 0x001C)

#define AXIM_STS		(VPP_REG_BASE + 0x002C)

#define DITH_PATH_CFG	(VPP_REG_BASE + 0x1000)
#define DITH_PATH_START	(VPP_REG_BASE + 0x1004)
#define DITH_IMG_SIZE	(VPP_REG_BASE + 0x1008)
#define DITH_MEM_SW		(VPP_REG_BASE + 0x100C)
#define DITH_SRC_ADDR	(VPP_REG_BASE + 0x1010)
#define DITH_DES_ADDR	(VPP_REG_BASE + 0x1014)
#define RB_COEF_CFG0	(VPP_REG_BASE + 0x1018)
#define RB_COEF_CFG1	(VPP_REG_BASE + 0x101C)
#define RB_COEF_CFG2	(VPP_REG_BASE + 0x1020)
#define RB_COEF_CFG3	(VPP_REG_BASE + 0x1024)
#define G_COEF_CFG0		(VPP_REG_BASE + 0x1028)
#define G_COEF_CFG1		(VPP_REG_BASE + 0x102C)
#define G_COEF_CFG2		(VPP_REG_BASE + 0x1030)
#define G_COEF_CFG3		(VPP_REG_BASE + 0x1034)

#define DITH_PATH_TABLE	(VPP_REG_BASE + 0x1400)

#define MM_ENABLE		0x402E0000
#define VPP_ENABLE		0x60D00000
#define AXI_ENABLE		0x60D00008

#endif

