/* the head file modifier:     ang   2015-01-21 14:34:02*/

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
#error  "Don't include this file directly, Pls include sci_glb_regs.h"
#endif


#ifndef __H_REGS_CODEC_AHB_HEADFILE_H__
#define __H_REGS_CODEC_AHB_HEADFILE_H__ __FILE__

#define  REGS_CODEC_AHB

/* registers definitions for CODEC_AHB */
#define REG_CODEC_AHB_AHB_RST				SCI_ADDR(REGS_CODEC_AHB_BASE, 0x0000)/*AHB_RST*/
#define REG_CODEC_AHB_CKG_ENABLE			SCI_ADDR(REGS_CODEC_AHB_BASE, 0x0004)/*CKG_ENABLE*/
#define REG_CODEC_AHB_CLOCK_SEL				SCI_ADDR(REGS_CODEC_AHB_BASE, 0x0008)/*CLOCK_SEL*/
#define REG_CODEC_AHB_CODA7_STAT			SCI_ADDR(REGS_CODEC_AHB_BASE, 0x000C)/*CODA7_STAT*/



/* bits definitions for register REG_CODEC_AHB_AHB_RST */
#define BIT_CODA7_APB_SOFT_RST					(BIT(2))
#define BIT_CODA7_CC_SOFT_RST					(BIT(1))
#define BIT_CODA7_AXI_SOFT_RST					(BIT(0))

/* bits definitions for register REG_CODEC_AHB_CKG_ENABLE */
#define BIT_CODA7_CKG_EN					(BIT(4))
#define BIT_CODA7_CC_CKG_EN					(BIT(1))
#define BIT_CODA7_AXI_CKG_EN					(BIT(0))

/* bits definitions for register REG_CODEC_AHB_CLOCK_SEL */
#define BITS_CLK_CODA7_AXI_SEL(_X_)				((_X_) << 8 & (BIT(8)|BIT(9)))
#define BITS_CLK_CODA7_CC_SEL(_X_)				((_X_) << 4 & (BIT(4)|BIT(5)))
#define BITS_CLK_CODA7_APB_SEL(_X_)				((_X_) & (BIT(0)|BIT(1)))

/* bits definitions for register REG_CODEC_AHB_CODA7_STAT */
#define BIT_CODA7_RUN						(BIT(1))
#define BIT_CODA7_IDLE						(BIT(0))

#endif
