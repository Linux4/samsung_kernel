/* the head file modifier:     g   2015-03-26 16:07:52*/

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


#ifndef __H_REGS_DISP_CLK_HEADFILE_H__
#define __H_REGS_DISP_CLK_HEADFILE_H__ __FILE__


/* registers definitions for DISP_CLK */
#define REG_DISP_CLK_CGM_AHB_DISP_CFG       SCI_ADDR(REGS_DISP_CLK_BASE, 0x0020 )
#define REG_DISP_CLK_CGM_GSP0_CFG           SCI_ADDR(REGS_DISP_CLK_BASE, 0x0024 )
#define REG_DISP_CLK_CGM_GSP1_CFG           SCI_ADDR(REGS_DISP_CLK_BASE, 0x0028 )
#define REG_DISP_CLK_CGM_DISPC0_CFG         SCI_ADDR(REGS_DISP_CLK_BASE, 0x002C )
#define REG_DISP_CLK_CGM_DISPC0_DBI_CFG     SCI_ADDR(REGS_DISP_CLK_BASE, 0x0030 )
#define REG_DISP_CLK_CGM_DISPC0_DPI_CFG     SCI_ADDR(REGS_DISP_CLK_BASE, 0x0034 )
#define REG_DISP_CLK_CGM_DISPC1_CFG         SCI_ADDR(REGS_DISP_CLK_BASE, 0x0038 )
#define REG_DISP_CLK_CGM_DISPC1_DBI_CFG     SCI_ADDR(REGS_DISP_CLK_BASE, 0x003C )
#define REG_DISP_CLK_CGM_DISPC1_DPI_CFG     SCI_ADDR(REGS_DISP_CLK_BASE, 0x0040 )
#define REG_DISP_CLK_CGM_DSI0_RXESC_CFG     SCI_ADDR(REGS_DISP_CLK_BASE, 0x0044 )
#define REG_DISP_CLK_CGM_DSI0_LANEBYTE_CFG  SCI_ADDR(REGS_DISP_CLK_BASE, 0x0048 )
#define REG_DISP_CLK_CGM_DPHY0_CFG_CFG      SCI_ADDR(REGS_DISP_CLK_BASE, 0x004C )
#define REG_DISP_CLK_CGM_DSI1_RXESC_CFG     SCI_ADDR(REGS_DISP_CLK_BASE, 0x0050 )
#define REG_DISP_CLK_CGM_DSI1_LANEBYTE_CFG  SCI_ADDR(REGS_DISP_CLK_BASE, 0x0054 )
#define REG_DISP_CLK_CGM_DPHY1_CFG_CFG      SCI_ADDR(REGS_DISP_CLK_BASE, 0x0058 )
#define REG_DISP_CLK_CGM_VPP_CFG            SCI_ADDR(REGS_DISP_CLK_BASE, 0x005C )
#define REG_DISP_CLK_CGM_BIST_400M_CFG      SCI_ADDR(REGS_DISP_CLK_BASE, 0x0060 )

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_AHB_DISP_CFG
// Register Offset : 0x0020
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_AHB_DISP_SEL(x)                      (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_GSP0_CFG
// Register Offset : 0x0024
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_GSP0_SEL(x)                          (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_GSP1_CFG
// Register Offset : 0x0028
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_GSP1_SEL(x)                          (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DISPC0_CFG
// Register Offset : 0x002C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DISPC0_DIV(x)                        (((x) & 0x7) << 8)
#define BIT_DISP_CLK_CGM_DISPC0_SEL(x)                        (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DISPC0_DBI_CFG
// Register Offset : 0x0030
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DISPC0_DBI_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_DISP_CLK_CGM_DISPC0_DBI_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DISPC0_DPI_CFG
// Register Offset : 0x0034
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DISPC0_DPI_PAD_SEL                   BIT(16)
#define BIT_DISP_CLK_CGM_DISPC0_DPI_DIV(x)                    (((x) & 0xFF) << 8)
#define BIT_DISP_CLK_CGM_DISPC0_DPI_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DISPC1_CFG
// Register Offset : 0x0038
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DISPC1_DIV(x)                        (((x) & 0x7) << 8)
#define BIT_DISP_CLK_CGM_DISPC1_SEL(x)                        (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DISPC1_DBI_CFG
// Register Offset : 0x003C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DISPC1_DBI_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_DISP_CLK_CGM_DISPC1_DBI_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DISPC1_DPI_CFG
// Register Offset : 0x0040
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DISPC1_DPI_DIV(x)                    (((x) & 0xFF) << 8)
#define BIT_DISP_CLK_CGM_DISPC1_DPI_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DSI0_RXESC_CFG
// Register Offset : 0x0044
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DSI0_RXESC_PAD_SEL                   BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DSI0_LANEBYTE_CFG
// Register Offset : 0x0048
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DSI0_LANEBYTE_PAD_SEL                BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DPHY0_CFG_CFG
// Register Offset : 0x004C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DPHY0_CFG_SEL                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DSI1_RXESC_CFG
// Register Offset : 0x0050
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DSI1_RXESC_PAD_SEL                   BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DSI1_LANEBYTE_CFG
// Register Offset : 0x0054
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DSI1_LANEBYTE_PAD_SEL                BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_DPHY1_CFG_CFG
// Register Offset : 0x0058
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_DPHY1_CFG_SEL                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_VPP_CFG
// Register Offset : 0x005C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_VPP_SEL(x)                           (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_DISP_CLK_CGM_BIST_400M_CFG
// Register Offset : 0x0060
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DISP_CLK_CGM_BIST_400M_SEL                        BIT(0)

#endif
