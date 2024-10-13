/* the head file modifier:        2015-06-29 13:24:33*/

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


#ifndef __H_REGS_HEADFILE_H__
#define __H_REGS_HEADFILE_H__ __FILE__

#define  REGS_AGCP_AHB

/* registers definitions for AGCP_AHB_RF */
#define REG_AGCP_AHB_MODULE_EB0_STS				SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0000)/*MODULE_EB0_STS*/
#define REG_AGCP_AHB_MODULE_EB1_STS				SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0004)/*MODULE_EB1_STS*/
#define REG_AGCP_AHB_MODULE_RST0_STS			SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0008)/*MODULE_RST0_STS*/
#define REG_AGCP_AHB_BUS_CTRL					SCI_ADDR(REGS_AGCP_AHB_BASE, 0x000C)/*BUS_CTRL*/
#define REG_AGCP_AHB_ACC_AUTO					SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0010)/*ACC_AUTO*/
#define REG_AGCP_AHB_DMA_CLK_EN					SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0014)/*DMA_CLK_EN*/
#define REG_AGCP_AHB_AHB_ARCH_EB				SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0018)/*AHB_ARCH_EB*/
#define REG_AGCP_AHB_CORE_SLEEP					SCI_ADDR(REGS_AGCP_AHB_BASE, 0x001C)/*CORE_SLEEP*/
#define REG_AGCP_AHB_SYS_SLP_EN					SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0020)/*SYS_SLP_EN*/
#define REG_AGCP_AHB_MCACHE_CTRL				SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0024)/*MCACHE_CTRL*/
#define REG_AGCP_AHB_MCU_STATUS					SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0028)/*MCU_STATUS*/
#define REG_AGCP_AHB_ID2QOS_AR_0				SCI_ADDR(REGS_AGCP_AHB_BASE, 0x002C)/*ID2QOS_AR_0*/
#define REG_AGCP_AHB_ID2QOS_AR_1				SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0030)/*ID2QOS_AR_1*/
#define REG_AGCP_AHB_ID2QOS_AW_0				SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0034)/*ID2QOS_AW_0*/
#define REG_AGCP_AHB_ID2QOS_AW_1				SCI_ADDR(REGS_AGCP_AHB_BASE, 0x0038)/*ID2QOS_AW_1*/
#define REG_AGCP_AHB_AG_RESERVED				SCI_ADDR(REGS_AGCP_AHB_BASE, 0x003C)/*AG_RESERVED*/



/* bits definitions for register REG_AGCP_AHB_MODULE_EB0_STS */
#define BIT_DMA_CP_ASHB_EB				( BIT(18) )
#define BIT_DMA_AP_ASHB_EB				( BIT(17) )
#define BIT_ICU_EB					( BIT(16) )
#define BIT_SPINLOCK_EB				( BIT(15) )
#define BIT_VBC_EB					( BIT(14) )
#define BIT_VBCIFD_EB				( BIT(13) )
#define BIT_MCDT_EB					( BIT(12) )
#define BIT_SRC44P1K_EB				( BIT(11) )
#define BIT_SRC48K_EB				( BIT(10) )
#define BIT_DMA_AP_EB				( BIT(6) )
#define BIT_DMA_CP_EB				( BIT(5) )
#define BIT_UART_EB					( BIT(4) )
#define BIT_IIS3_EB					( BIT(3) )
#define BIT_IIS2_EB					( BIT(2) )
#define BIT_IIS1_EB					( BIT(1) )
#define BIT_IIS0_EB					( BIT(0) )

/* bits definitions for register REG_MODULE_EB1_STS */
#define BIT_CLK_ALL_EN				( BIT(24) )
#define BIT_CX_CLKRFT_EN				( BIT(15) )
#define BIT_CLK_GSMCAL_EN				( BIT(14) )
#define BIT_CLK_CDC_EN				( BIT(13) )
#define BIT_CLK_8PSK_EN				( BIT(12) )
#define BIT_CLK_BTX_EN				( BIT(11) )
#define BIT_CLK_LSE_EN				( BIT(10) )
#define BIT_CLK_GENCORR_EN				( BIT(9) )
#define BIT_CLK_MAP_EN				( BIT(8) )
#define BIT_CLK_VE_EN				( BIT(7) )
#define BIT_BRX_EB					( BIT(6) )
#define BIT_CDC_EB					( BIT(5) )
#define BIT_BTX_EB					( BIT(4) )
#define BIT_LSE_EB					( BIT(3) )
#define BIT_GENCORR_EB				( BIT(2) )
#define BIT_MAP_EB					( BIT(1) )
#define BIT_SAIC_EB					( BIT(0) )

/* bits definitions for register REG_MODULE_RST0_STS */
#define BIT_GCORR_SOFT_RST				( BIT(24) )
#define BIT_LSE_SOFT_RST				( BIT(23) )
#define BIT_MAP_SOFT_RST				( BIT(22) )
#define BIT_PSK8_SOFT_RST				( BIT(21) )
#define BIT_CDC_SOFT_RST				( BIT(20) )
#define BIT_VE_SOFT_RST				( BIT(19) )
#define BIT_BRX_SOFT_RST				( BIT(18) )
#define BIT_BTX_SOFT_RST				( BIT(17) )
#define BIT_UART_SOFT_RST				( BIT(16) )
#define BIT_IIS3_SOFT_RST				( BIT(15) )
#define BIT_IIS2_SOFT_RST				( BIT(14) )
#define BIT_IIS1_SOFT_RST				( BIT(13) )
#define BIT_IIS0_SOFT_RST				( BIT(12) )
#define BIT_DMA_CP_SOFT_RST				( BIT(11) )
#define BIT_SPINLOCK_SOFT_RST			( BIT(10) )
#define BIT_VBC_SOFT_RST				( BIT(9) )
#define BIT_VBCIFD_SOFT_RST				( BIT(8) )
#define BIT_MCDT_SOFT_RST				( BIT(7) )
#define BIT_SRC44P1K_SOFT_RST			( BIT(6) )
#define BIT_SRC48K_SOFT_RST				( BIT(5) )
#define BIT_DMA_AP_SOFT_RST				( BIT(1) )

/* bits definitions for register REG_BUS_CTRL */
#define BIT_M0_NIU_IDLE_DET_DIS			( BIT(8) )
#define BIT_RST_GSM_ACC				( BIT(3) )
#define BIT_ZBUS_32ACCESS				( BIT(0) )

/* bits definitions for register REG_ACC_AUTO */
#define BITS_ACC_AUTO_EN(_X_)			( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register REG_DMA_CLK_EN */
#define BIT_SRC44P1K_LSLP_EN			( BIT(6) )
#define BIT_DMA_CP_LSLP_EN				( BIT(5) )
#define BIT_DMA_AP_LSLP_EN				( BIT(4) )
#define BIT_PMU_CPDMA_FRC_SLP			( BIT(3) )
#define BIT_PMU_APDMA_FRC_SLP			( BIT(2) )
#define BIT_CPDMA_CLK_AUTO_EN			( BIT(1) )
#define BIT_APDMA_CLK_AUTO_EN			( BIT(0) )

/* bits definitions for register REG_AHB_ARCH_EB */
#define BIT_ACCZ_ARCH_EB				( BIT(1) )
#define BIT_AHB_ARCH_EB				( BIT(0) )

/* bits definitions for register REG_CORE_SLEEP */
#define BIT_DSP_CORE_SLEEP				( BIT(0) )

/* bits definitions for register REG_SYS_SLP_EN */
#define BIT_DSP_SYS_SLEEP_EN			( BIT(0) )

/* bits definitions for register REG_MCACHE_CTRL */
#define BITS_MCACHE_STRAP_POST_NUM(_X_)		( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_MCACHE_STRAP_PRE_NUM(_X_)		( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_MCU_STATUS */
#define BITS_MCU_STATUS(_X_)			( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )

/* bits definitions for register REG_ID2QOS_AR_0 */
#define BITS_AR_QOS_ID7(_X_)			( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_AR_QOS_ID6(_X_)			( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_AR_QOS_ID5(_X_)			( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_AR_QOS_ID4(_X_)			( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_AR_QOS_ID3(_X_)			( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_AR_QOS_ID2(_X_)			( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_AR_QOS_ID1(_X_)			( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_AR_QOS_ID0(_X_)			( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ID2QOS_AR_1 */
#define BIT_AR_ID_SEL				( BIT(5) )
#define BIT_AR_QOS_SEL				( BIT(4) )
#define BITS_AR_QOS(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ID2QOS_AW_0 */
#define BITS_AW_QOS_ID7(_X_)			( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_AW_QOS_ID6(_X_)			( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_AW_QOS_ID5(_X_)			( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_AW_QOS_ID4(_X_)			( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_AW_QOS_ID3(_X_)			( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_AW_QOS_ID2(_X_)			( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_AW_QOS_ID1(_X_)			( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_AW_QOS_ID0(_X_)			( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ID2QOS_AW_1 */
#define BIT_AW_ID_SEL				( BIT(5) )
#define BIT_AW_QOS_SEL				( BIT(4) )
#define BITS_AW_QOS(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AG_RESERVED */
#define BITS_HIGH_RESERVED(_X_)			( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_LOW_RESERVED(_X_)			( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

#endif

