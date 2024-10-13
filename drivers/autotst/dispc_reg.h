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

#ifndef _DISPC_REG_H_
#define _DISPC_REG_H_

#include <asm/io.h>
#include <mach/sci.h>
#include <mach/hardware.h>


/* DISPC regs offset */
#define DISPC_CTRL		(0x0000)
#define DISPC_SIZE_XY		(0x0004)
#define DISPC_RSTN		(0x0008)

#define DISPC_STS			(0x0010)

#define DISPC_IMG_CTRL			(0x0020)
#define DISPC_IMG_Y_BASE_ADDR	(0x0024)
#define DISPC_IMG_UV_BASE_ADDR	(0x0028)
#define DISPC_IMG_V_BASE_ADDR	(0x002c)
#define DISPC_IMG_SIZE_XY			(0x0030)
#define DISPC_IMG_PITCH			(0x0034)
#define DISPC_IMG_DISP_XY			(0x0038)
#define DISPC_BG_COLOR			(0x003c)

#define DISPC_OSD_CTRL		(0x0040)
#define DISPC_OSD_BASE_ADDR	(0x0044)
#define DISPC_OSD_SIZE_XY		(0x0048)
#define DISPC_OSD_PITCH		(0x004c)
#define DISPC_OSD_DISP_XY		(0x0050)
#define DISPC_OSD_ALPHA		(0x0054)
#define DISPC_OSD_CK			(0x0058)

#define DISPC_Y2R_CTRL			(0x0060)
#define DISPC_Y2R_CONTRAST		(0x0064)
#define DISPC_Y2R_SATURATION		(0x0068)
#define DISPC_Y2R_BRIGHTNESS		(0x006c)

#define DISPC_INT_EN			(0x0070)
#define DISPC_INT_CLR			(0x0074)
#define DISPC_INT_STATUS		(0x0078)
#define DISPC_INT_RAW		(0x007c)

#define DISPC_DPI_CTRL		(0x0080)
#define DISPC_DPI_H_TIMING	(0x0084)
#define DISPC_DPI_V_TIMING	(0x0088)
#define DISPC_DPI_STS0		(0x008c)
#define DISPC_DPI_STS1		(0x0090)

#define DISPC_DBI_CTRL		(0x00a0)
#define DISPC_DBI_TIMING0		(0x00a4)
#define DISPC_DBI_TIMING1		(0x00a8)
#define DISPC_DBI_RDATA		(0x00ac)
#define DISPC_DBI_CMD		(0x00b0)
#define DISPC_DBI_DATA		(0x00b4)
#define DISPC_DBI_QUEUE		(0x00b8)



//shadow register , read only
#define SHDW_IMG_CTRL				(0x00C0)
#define SHDW_IMG_Y_BASE_ADDR		(0x00C4)
#define SHDW_IMG_UV_BASE_ADDR		(0x00C8)
#define SHDW_IMG_V_BASE_ADDR		(0x00CC)
#define SHDW_IMG_SIZE_XY			(0x00D0)
#define SHDW_IMG_PITCH				(0x00D4)
#define SHDW_IMG_DISP_XY			(0x00D8)
#define SHDW_BG_COLOR				(0x00DC)
#define SHDW_OSD_CTRL				(0x00E0)
#define SHDW_OSD_BASE_ADDR			(0x00E4)
#define SHDW_OSD_SIZE_XY			(0x00E8)
#define SHDW_OSD_PITCH				(0x00EC)
#define SHDW_OSD_DISP_XY			(0x00F0)
#define SHDW_OSD_ALPHA				(0x00F4)
#define SHDW_OSD_CK					(0x00F8)
#define SHDW_Y2R_CTRL				(0x0100)
#define SHDW_Y2R_CONTRAST			(0x0104)
#define SHDW_Y2R_SATURATION			(0x0108)
#define SHDW_Y2R_BRIGHTNESS			(0x010C)
#define SHDW_DPI_H_TIMING			(0x0110)
#define SHDW_DPI_V_TIMING			(0x0114)
#define DISPC_TE_SYNC_DELAY	(0x00bc)

#define SPRD_DISPC_BASE			SPRD_LCDC_BASE
#define	IRQ_DISPC_INT			IRQ_DISPC0_INT

#define REG_AHB_SOFT_RST 			(0x4 + SPRD_AHB_BASE)
#define BIT_DISPC_SOFT_RST			BIT_DISPC0_SOFT_RST

#define DISPC_CORE_EN			(REG_AP_APB_APB_EB)
#define BIT_DISPC_CORE_EN			(BIT_AP_CKG_EB)

enum{
	DISPC_IF_DBI = 0,
	DISPC_IF_DPI,
	DISPC_IF_EDPI,
	DISPC_IF_LIMIT
};

static inline uint32_t dispc_read(uint32_t reg)
{
	return sci_glb_read(SPRD_DISPC_BASE + reg, 0xffffffff);
}

static inline void dispc_write(uint32_t value, uint32_t reg)
{
	sci_glb_write((SPRD_DISPC_BASE + reg), value, 0xffffffff);
}

static inline void dispc_set_bits(uint32_t bits, uint32_t reg)
{
	dispc_write(dispc_read(reg) | bits, reg);
}

static inline void dispc_clear_bits(uint32_t bits, uint32_t reg)
{
	dispc_write(dispc_read(reg) & ~bits, reg);
}

#endif
