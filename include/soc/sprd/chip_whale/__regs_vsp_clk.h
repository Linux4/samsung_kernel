/* the head file modifier:     g   2015-03-26 16:11:15*/

/*
* Copyright (C) 2013 Spreadtrum Communications Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*************************************************
* Automatically generated C header: do not edit *
*************************************************
*/

#ifndef __SCI_GLB_REGS_H__
#error "Don't include this file directly, Pls include sci_glb_regs.h"
#endif


#ifndef __H_REGS_VSP_CLK_HEADFILE_H__
#define __H_REGS_VSP_CLK_HEADFILE_H__ __FILE__

#define  REGS_VSP_CLK
#define  REGS_VSP_CLK_BASE SPRD_VSPCKG_BASE

/* registers definitions for VSP_CLK */
#define REG_VSP_CLK_cgm_ahb_vsp_cfg			SCI_ADDR(REGS_VSP_CLK_BASE, 0x20)/*cgm_ahb_vsp_cfg*/
#define REG_VSP_CLK_cgm_vsp_cfg				SCI_ADDR(REGS_VSP_CLK_BASE, 0x24)/*cgm_vsp_cfg*/
#define REG_VSP_CLK_cgm_orp_tck_cfg			SCI_ADDR(REGS_VSP_CLK_BASE, 0x28)/*cgm_orp_tck_cfg*/
#define REG_VSP_CLK_cgm_bist_400m_cfg			SCI_ADDR(REGS_VSP_CLK_BASE, 0x2c)/*cgm_bist_400m_cfg*/



/* bits definitions for register REG_VSP_CLK_cgm_ahb_vsp_cfg */
#define BITS_VSP_CLK_CGM_AHB_VSP_SEL(_X_)			((_X_) & (BIT(0)|BIT(1)))

/* bits definitions for register REG_VSP_CLK_cgm_vsp_cfg */
#define BITS_VSP_CLK_CGM_VSP_DIV(_X_)				((_X_) << 8 & (BIT(8)|BIT(9)))
#define BITS_VSP_CLK_CGM_VSP_SEL(_X_)				((_X_) & (BIT(0)|BIT(1)))

/* bits definitions for register REG_VSP_CLK_cgm_orp_tck_cfg */
#define BIT_VSP_CLK_CGM_ORP_TCK_PAD_SEL				(BIT(16))

/* bits definitions for register REG_VSP_CLK_cgm_bist_400m_cfg */
#define BIT_VSP_CLK_CGM_BIST_400M_SEL				(BIT(0))

#endif
