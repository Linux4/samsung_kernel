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


#ifndef __H_REGS_CRYPTO_APB_HEADFILE_H__
#define __H_REGS_CRYPTO_APB_HEADFILE_H__ __FILE__


/* registers definitions for CRYPTO_APB_RF */
#define REG_CRYPTO_APB_RF_APB_EB	SCI_ADDR(REGS_CRYPTO_APB_RF_BASE, 0x0000)
#define REG_CRYPTO_APB_RF_APB_RST	SCI_ADDR(REGS_CRYPTO_APB_RF_BASE, 0x0004)
#define REG_CRYPTO_APB_RF_CLK_SEL	SCI_ADDR(REGS_CRYPTO_APB_RF_BASE, 0x0008)



/* bits definitions for register REG_CRYPTO_APB_RF_APB_EB */
#define BIT_CRYPTO_APB_RF_CE_AXI_EB				(BIT(0))

/* bits definitions for register REG_CRYPTO_APB_RF_APB_RST */
#define BIT_CRYPTO_APB_RF_CE_SOFT_RST				(BIT(0))

/* bits definitions for register REG_CRYPTO_APB_RF_CLK_SEL */
#define BITS_CRYPTO_APB_RF_CLK_CE_SEL(_X_)	((_X_) & (BIT(0) | BIT(1)))
#endif
