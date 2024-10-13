/* the head file modifier:     g   2015-03-26 16:06:59*/

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


#ifndef __H_REGS_CAM_CLK_HEADFILE_H__
#define __H_REGS_CAM_CLK_HEADFILE_H__ __FILE__
#define  REGS_CAM_CLK_BASE SPRD_MMCKG_BASE

#define REG_CAM_CLK_CGM_AHB_CAM_CFG    SCI_ADDR(REGS_CAM_CLK_BASE, 0x0020 )
#define REG_CAM_CLK_CGM_SENSOR0_CFG    SCI_ADDR(REGS_CAM_CLK_BASE, 0x0024 )
#define REG_CAM_CLK_CGM_SENSOR1_CFG    SCI_ADDR(REGS_CAM_CLK_BASE, 0x0028 )
#define REG_CAM_CLK_CGM_DCAM0_CFG      SCI_ADDR(REGS_CAM_CLK_BASE, 0x002C )
#define REG_CAM_CLK_CGM_DCAM1_CFG      SCI_ADDR(REGS_CAM_CLK_BASE, 0x0030 )
#define REG_CAM_CLK_CGM_ISP_CFG        SCI_ADDR(REGS_CAM_CLK_BASE, 0x0034 )
#define REG_CAM_CLK_CGM_JPG0_CFG       SCI_ADDR(REGS_CAM_CLK_BASE, 0x0038 )
#define REG_CAM_CLK_CGM_JPG1_CFG       SCI_ADDR(REGS_CAM_CLK_BASE, 0x003C )
#define REG_CAM_CLK_CGM_MIPI_CSI0_CFG  SCI_ADDR(REGS_CAM_CLK_BASE, 0x0040 )
#define REG_CAM_CLK_CGM_MIPI_CSI1_CFG  SCI_ADDR(REGS_CAM_CLK_BASE, 0x0044 )
#define REG_CAM_CLK_CGM_CPHY0_CFG_CFG  SCI_ADDR(REGS_CAM_CLK_BASE, 0x0048 )
#define REG_CAM_CLK_CGM_CPHY1_CFG_CFG  SCI_ADDR(REGS_CAM_CLK_BASE, 0x004C )
#define REG_CAM_CLK_CGM_BIST_400M_CFG  SCI_ADDR(REGS_CAM_CLK_BASE, 0x0050 )

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_AHB_CAM_CFG
// Register Offset : 0x0020
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_AHB_CAM_SEL(x)                  (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_SENSOR0_CFG
// Register Offset : 0x0024
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_SENSOR0_DIV(x)                  (((x) & 0x7) << 8)
#define BIT_CAM_CLK_CGM_SENSOR0_SEL(x)                  (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_SENSOR1_CFG
// Register Offset : 0x0028
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_SENSOR1_DIV(x)                  (((x) & 0x7) << 8)
#define BIT_CAM_CLK_CGM_SENSOR1_SEL(x)                  (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_DCAM0_CFG
// Register Offset : 0x002C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_DCAM0_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_DCAM1_CFG
// Register Offset : 0x0030
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_DCAM1_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_ISP_CFG
// Register Offset : 0x0034
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_ISP_SEL(x)                      (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_JPG0_CFG
// Register Offset : 0x0038
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_JPG0_SEL(x)                     (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_JPG1_CFG
// Register Offset : 0x003C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_JPG1_SEL(x)                     (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_MIPI_CSI0_CFG
// Register Offset : 0x0040
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_MIPI_CSI0_PAD_SEL               BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_MIPI_CSI1_CFG
// Register Offset : 0x0044
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_MIPI_CSI1_PAD_SEL               BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_CPHY0_CFG_CFG
// Register Offset : 0x0048
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_CPHY0_CFG_SEL                   BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_CPHY1_CFG_CFG
// Register Offset : 0x004C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_CPHY1_CFG_SEL                   BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_CLK_CGM_BIST_400M_CFG
// Register Offset : 0x0050
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_CLK_CGM_BIST_400M_SEL                   BIT(0)

#endif
