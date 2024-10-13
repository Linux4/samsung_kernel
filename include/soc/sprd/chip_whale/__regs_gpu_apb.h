/* the head file modifier:     g   2015-03-26 15:25:38*/

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


#ifndef __H_REGS_GPU_APB_HEADFILE_H__
#define __H_REGS_GPU_APB_HEADFILE_H__ __FILE__



#define REG_GPU_APB_APB_RST                  SCI_ADDR(REGS_GPU_APB_BASE, 0x0000 )
#define REG_GPU_APB_GPU_PATH_SEL             SCI_ADDR(REGS_GPU_APB_BASE, 0x0004 )
#define REG_GPU_APB_GPU_BARRIER_DISABLE      SCI_ADDR(REGS_GPU_APB_BASE, 0x0008 )
#define REG_GPU_APB_GPU_PATH                 SCI_ADDR(REGS_GPU_APB_BASE, 0x000C )
#define REG_GPU_APB_GPUD_ACTIVE              SCI_ADDR(REGS_GPU_APB_BASE, 0x0010 )

/*---------------------------------------------------------------------------
// Register Name   : REG_GPU_APB_APB_RST
// Register Offset : 0x0000
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_GPU_APB_GPU_SOFT_RST                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_GPU_APB_GPU_PATH_SEL
// Register Offset : 0x0004
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_GPU_APB_GPU_PATH_SEL                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_GPU_APB_GPU_BARRIER_DISABLE
// Register Offset : 0x0008
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_GPU_APB_GPU_BARRIER_DISABLE               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_GPU_APB_GPU_PATH
// Register Offset : 0x000C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_GPU_APB_GPU_PATH                          BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_GPU_APB_GPUD_ACTIVE
// Register Offset : 0x0010
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_GPU_APB_GDC_ACTIVE                        BIT(2)
#define BIT_GPU_APB_GDG_ACTIVE                        BIT(1)
#define BIT_GPU_APB_GDL_ACTIVE                        BIT(0)

#endif
