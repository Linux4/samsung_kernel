/* the head file modifier:     g   2015-03-26 15:27:58*/

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


#ifndef __H_REGS_VSP_AHB_HEADFILE_H__
#define __H_REGS_VSP_AHB_HEADFILE_H__ __FILE__

#define  REGS_VSP_AHB_BASE SPRD_VSPAHB_BASE

#define REG_VSP_AHB_AHB_EB       SCI_ADDR(REGS_VSP_AHB_BASE, 0x0000 )
#define REG_VSP_AHB_AHB_RST      SCI_ADDR(REGS_VSP_AHB_BASE, 0x0004 )
#define REG_VSP_AHB_GEN_CKG_CFG  SCI_ADDR(REGS_VSP_AHB_BASE, 0x0008 )

/*---------------------------------------------------------------------------
// Register Name   : REG_VSP_AHB_AHB_EB
// Register Offset : 0x0000
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_VSP_AHB_MMU_EB                            BIT(2)
#define BIT_VSP_AHB_CKG_EB                            BIT(1)
#define BIT_VSP_AHB_VSP_EB                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_VSP_AHB_AHB_RST
// Register Offset : 0x0004
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_VSP_AHB_MMU_SOFT_RST                      BIT(3)
#define BIT_VSP_AHB_CKG_SOFT_RST                      BIT(2)
#define BIT_VSP_AHB_OR1200_SOFT_RST                   BIT(1)
#define BIT_VSP_AHB_VSP_SOFT_RST                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_VSP_AHB_GEN_CKG_CFG
// Register Offset : 0x0008
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_VSP_AHB_VSP_AXI_CKG_EN                    BIT(0)

#endif
