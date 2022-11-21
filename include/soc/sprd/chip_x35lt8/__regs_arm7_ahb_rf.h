/* the head file modifier:     g   2015-03-19 15:41:25*/

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


#ifndef __H_REGS_ARM7_AHB_RF_HEADFILE_H__
#define __H_REGS_ARM7_AHB_RF_HEADFILE_H__ __FILE__

#define  REGS_ARM7_AHB_RF

/* registers definitions for ARM7_AHB_RF */
#define REG_ARM7_AHB_RF_ARM7_EB				SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0000)/*ARM7_EB*/
#define REG_ARM7_AHB_RF_ARM7_SOFT_RST			SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0004)/*ARM7_SOFT_RST*/
#define REG_ARM7_AHB_RF_AHB_PAUSE			SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0008)/*AHB_PAUSE*/
#define REG_ARM7_AHB_RF_ARM7_SLP_CTL			SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x000C)/*ARM7_SLP_CTL*/
#define REG_ARM7_AHB_RF_CLK_EMC4X_CFG			SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0010)/*CLK_EMC4X_CFG*/
#define REG_ARM7_AHB_RF_APCPU_LIT_RVBADDR0		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0020)/*APCPU_LIT_RVBADDR0*/
#define REG_ARM7_AHB_RF_APCPU_LIT_RVBADDR1		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0024)/*APCPU_LIT_RVBADDR1*/
#define REG_ARM7_AHB_RF_APCPU_LIT_RVBADDR2		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0028)/*APCPU_LIT_RVBADDR2*/
#define REG_ARM7_AHB_RF_APCPU_LIT_RVBADDR3		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x002C)/*APCPU_LIT_RVBADDR3*/
#define REG_ARM7_AHB_RF_APCPU_LIT_PROT_CTL		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0030)/*APCPU_LIT_PROT_CTL*/
#define REG_ARM7_AHB_RF_APCPU_LIT_COMM_CTL		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0034)/*APCPU_LIT_COMM_CTL*/
#define REG_ARM7_AHB_RF_ARM7_RES_REG			SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0044)/*ARM7_RES_REG*/
#define REG_ARM7_AHB_RF_APCPU_BIG_RVBADDR0		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0050)/*APCPU_BIG_RVBADDR0*/
#define REG_ARM7_AHB_RF_APCPU_BIG_RVBADDR1		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0054)/*APCPU_BIG_RVBADDR1*/
#define REG_ARM7_AHB_RF_APCPU_BIG_RVBADDR2		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0058)/*APCPU_BIG_RVBADDR2*/
#define REG_ARM7_AHB_RF_APCPU_BIG_RVBADDR3		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x005C)/*APCPU_BIG_RVBADDR3*/
#define REG_ARM7_AHB_RF_APCPU_BIG_PROT_CTL		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0060)/*APCPU_BIG_PROT_CTL*/
#define REG_ARM7_AHB_RF_APCPU_BIG_COMM_CTL		SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0064)/*APCPU_BIG_COMM_CTL*/
#define REG_ARM7_AHB_RF_CSSYS_PROT_CTL			SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0070)/*CSSYS_PROT_CTL*/
#define REG_ARM7_AHB_RF_CCI_PROT_CTL			SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0074)/*CCI_PROT_CTL*/



/* bits definitions for register REG_ARM7_AHB_RF_ARM7_EB */
#define BIT_ARM7_GPIO_EB					( BIT(10) )
#define BIT_ARM7_UART_EB					( BIT(9) )
#define BIT_ARM7_TMR_EB						( BIT(8) )
#define BIT_ARM7_SYST_EB					( BIT(7) )
#define BIT_ARM7_WDG_EB						( BIT(6) )
#define BIT_ARM7_EIC_EB						( BIT(5) )
#define BIT_ARM7_INTC_EB					( BIT(4) )
#define BIT_ARM7_EFUSE_EB					( BIT(3) )
#define BIT_ARM7_IMC_EB						( BIT(2) )
#define BIT_ARM7_TIC_EB						( BIT(1) )
#define BIT_ARM7_DMA_EB						( BIT(0) )

/* bits definitions for register REG_ARM7_AHB_RF_ARM7_SOFT_RST */
#define BIT_ARM7_GPIO_SOFT_RST					( BIT(10) )
#define BIT_ARM7_UART_SOFT_RST					( BIT(9) )
#define BIT_ARM7_TMR_SOFT_RST					( BIT(8) )
#define BIT_ARM7_SYST_SOFT_RST					( BIT(7) )
#define BIT_ARM7_WDG_SOFT_RST					( BIT(6) )
#define BIT_ARM7_EIC_SOFT_RST					( BIT(5) )
#define BIT_ARM7_INTC_SOFT_RST					( BIT(4) )
#define BIT_ARM7_EFUSE_SOFT_RST					( BIT(3) )
#define BIT_ARM7_IMC_SOFT_RST					( BIT(2) )
#define BIT_ARM7_TIC_SOFT_RST					( BIT(1) )
#define BIT_ARM7_ARCH_SOFT_RST					( BIT(0) )

/* bits definitions for register REG_ARM7_AHB_RF_AHB_PAUSE */
#define BIT_ARM7_DEEP_SLEEP_EN					( BIT(2) )
#define BIT_ARM7_SYS_SLEEP_EN					( BIT(1) )
#define BIT_ARM7_CORE_SLEEP					( BIT(0) )

/* bits definitions for register REG_ARM7_AHB_RF_ARM7_SLP_CTL */
#define BIT_ARM7_SYS_AUTO_GATE_EN				( BIT(2) )
#define BIT_ARM7_AHB_AUTO_GATE_EN				( BIT(1) )
#define BIT_ARM7_CORE_AUTO_GATE_EN				( BIT(0) )

/* bits definitions for register REG_ARM7_AHB_RF_CLK_EMC4X_CFG */
#define BITS_CLK_EMC4X_DIV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_CLK_EMC4X_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_LIT_RVBADDR0 */
#define BITS_APCPU_LIT_RVBADDR0(_X_)				(_X_)

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_LIT_RVBADDR1 */
#define BITS_APCPU_LIT_RVBADDR1(_X_)				(_X_)

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_LIT_RVBADDR2 */
#define BITS_APCPU_LIT_RVBADDR2(_X_)				(_X_)

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_LIT_RVBADDR3 */
#define BITS_APCPU_LIT_RVBADDR3(_X_)				(_X_)

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_LIT_PROT_CTL */
#define BITS_APCPU_LIT_SPNIDEN(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_APCPU_LIT_SPIDEN(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_APCPU_LIT_NIDEN(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_APCPU_LIT_DBGEN(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_LIT_COMM_CTL */
#define BIT_APCPU_LIT_DBGL1RSTDISABLE				( BIT(17) )
#define BIT_APCPU_LIT_L2RSTDISABLE				( BIT(16) )
#define BITS_APCPU_LIT_CFGTE(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_APCPU_LIT_AA64NAA32(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ARM7_AHB_RF_ARM7_RES_REG */
#define BITS_ARM7_RES_REG_H(_X_)				( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_ARM7_RES_REG_L(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_BIG_RVBADDR0 */
#define BITS_APCPU_BIG_RVBADDR0(_X_)				(_X_)

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_BIG_RVBADDR1 */
#define BITS_APCPU_BIG_RVBADDR1(_X_)				(_X_)

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_BIG_RVBADDR2 */
#define BITS_APCPU_BIG_RVBADDR2(_X_)				(_X_)

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_BIG_RVBADDR3 */
#define BITS_APCPU_BIG_RVBADDR3(_X_)				(_X_)

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_BIG_PROT_CTL */
#define BITS_APCPU_BIG_SPNIDEN(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_APCPU_BIG_SPIDEN(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_APCPU_BIG_NIDEN(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_APCPU_BIG_DBGEN(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ARM7_AHB_RF_APCPU_BIG_COMM_CTL */
#define BIT_APCPU_BIG_DBGL1RSTDISABLE				( BIT(17) )
#define BIT_APCPU_BIG_L2RSTDISABLE				( BIT(16) )
#define BITS_APCPU_BIG_CFGTE(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_APCPU_BIG_AA64NAA32(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ARM7_AHB_RF_CSSYS_PROT_CTL */
#define BIT_CSSYS_DAP_SPIDEN					( BIT(10) )
#define BIT_CSSYS_DAP_DBGEN					( BIT(8) )
#define BIT_CSSYS_DAP_DEVICEEN					( BIT(7) )
#define BIT_CSSYS_STM_NSGUAREN					( BIT(5) )
#define BIT_CSSYS_SPNIDEN					( BIT(3) )
#define BIT_CSSYS_SPIDEN					( BIT(2) )
#define BIT_CSSYS_NIDEN						( BIT(1) )
#define BIT_CSSYS_DBGEN						( BIT(0) )

/* bits definitions for register REG_ARM7_AHB_RF_CCI_PROT_CTL */
#define BIT_CCI_SPNIDEN						( BIT(3) )
#define BIT_CCI_NIDEN						( BIT(1) )

#endif
