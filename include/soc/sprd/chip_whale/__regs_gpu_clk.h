/* the head file modifier:     g   2015-03-26 16:08:10*/

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


#ifndef __H_REGS_GPU_CLK_HEADFILE_H__
#define __H_REGS_GPU_CLK_HEADFILE_H__ __FILE__



#define REG_GPU_CLK_CGM_GPU_CFG      SCI_ADDR(REGS_GPU_CLK_BASE, 0x0020 )
#define REG_GPU_CLK_CGM_GPU_26M_CFG  SCI_ADDR(REGS_GPU_CLK_BASE, 0x0024 )

/*---------------------------------------------------------------------------
// Register Name   : REG_GPU_CLK_CGM_GPU_CFG
// Register Offset : 0x0020
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_GPU_CLK_CGM_GPU_DIV(x)                (((x) & 0x3) << 8)
#define BIT_GPU_CLK_CGM_GPU_SEL(x)                (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_GPU_CLK_CGM_GPU_26M_CFG
// Register Offset : 0x0024
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_GPU_CLK_CGM_GPU_26M_SEL               BIT(0)

#endif
