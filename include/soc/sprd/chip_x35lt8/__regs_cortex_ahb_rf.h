/* the head file modifier:     g   2015-03-19 15:42:42*/

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


#ifndef __H_REGS_CORTEX_AHB_RF_HEADFILE_H__
#define __H_REGS_CORTEX_AHB_RF_HEADFILE_H__ __FILE__

#define  REGS_CORTEX_AHB_RF

/* registers definitions for CORTEX_AHB_RF */
#define REG_CORTEX_AHB_RF_CLK_CORTEX_AXI_CTRL		SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0000)/*CLK_CORTEX_AXI_CTRL*/
#define REG_CORTEX_AHB_RF_CLK_CORTEX_AHB_CTRL		SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0004)/*CLK_CORTEX_AHB_CTRL*/
#define REG_CORTEX_AHB_RF_CLK_CORTEX_APB_CTRL		SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0008)/*CLK_CORTEX_APB_CTRL*/
#define REG_CORTEX_AHB_RF_CLK_SEC0_CTRL			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x000C)/*CLK_SEC0_CTRL*/
#define REG_CORTEX_AHB_RF_CLK_SEC1_CTRL			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0010)/*CLK_SEC1_CTRL*/
#define REG_CORTEX_AHB_RF_CA5_CFG			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0030)/*CA5_CFG*/
#define REG_CORTEX_AHB_RF_AHB_CTRL0			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0034)/*AHB_CTRL0*/
#define REG_CORTEX_AHB_RF_AHB_CTRL2			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x003C)/*AHB_CTRL2*/
#define REG_CORTEX_AHB_RF_AHB_RST0_STS			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0060)/*AHB_RST0_STS*/
#define REG_CORTEX_AHB_RF_CA5_RST_SET			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0064)/*CA5_RST_SET*/
#define REG_CORTEX_AHB_RF_ARCH_EB			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0068)/*ARCH_EB*/
#define REG_CORTEX_AHB_RF_PTEST_FUNC			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x006C)/*REMAP*/
#define REG_CORTEX_AHB_RF_AHB_MISC_CTL			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0070)/*AHB_MISC_CTL*/
#define REG_CORTEX_AHB_RF_CA5_CTRL			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0074)/*CA5_CTRL*/
#define REG_CORTEX_AHB_RF_AUTO_GATE_CTRL		SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0078)/*AUTO_GATE_CTRL*/
#define REG_CORTEX_AHB_RF_AHB_PAUSE			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0080)/*AHB_PAUSE*/
#define REG_CORTEX_AHB_RF_AHB_SLP_CTL			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0084)/*AHB_SLP_CTL*/
#define REG_CORTEX_AHB_RF_AHB_SLP_STS			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0088)/*AHB_SLP_STS*/
#define REG_CORTEX_AHB_RF_MCU_CLK_CTL			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x008C)/*MCU_CLK_CTL*/
#define REG_CORTEX_AHB_RF_RES_REG0			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x00F4)/*RES_REG0*/
#define REG_CORTEX_AHB_RF_CHIP_ID			SCI_ADDR(REGS_CORTEX_AHB_RF_BASE, 0x0200)/*CHIP_ID*/



/* bits definitions for register REG_CORTEX_AHB_RF_CLK_CORTEX_AXI_CTRL */
#define BITS_CLK_CORTEX_AXI_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_AHB_RF_CLK_CORTEX_AHB_CTRL */
#define BITS_CLK_CORTEX_AHB_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_AHB_RF_CLK_CORTEX_APB_CTRL */
#define BITS_CLK_CORTEX_APB_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_AHB_RF_CLK_SEC0_CTRL */
#define BITS_CLK_SEC0_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_AHB_RF_CLK_SEC1_CTRL */
#define BITS_CLK_SEC1_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_AHB_RF_CA5_CFG */
#define BITS_CA5_DBG_DIV(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_CA5_AXI_DIV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_CA5_MCU_DIV(_X_)					( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_CA5_MCU_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_CORTEX_AHB_RF_AHB_CTRL0 */
#define BITS_TFT_MST_HPROT(_X_)					( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PPP1_MST_HPROT(_X_)				( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_PPP0_MST_HPROT(_X_)				( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BIT_TFT_EB						( BIT(18) )
#define BIT_PPP1_EB						( BIT(17) )
#define BIT_PPP0_EB						( BIT(16) )
#define BITS_SEC1_MST_HPROT(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_SEC0_MST_HPROT(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_SEC1_EB						( BIT(5) )
#define BIT_SEC1_CKG_EN						( BIT(4) )
#define BIT_SEC0_EB						( BIT(3) )
#define BIT_SEC0_CKG_EN						( BIT(2) )
#define BIT_DMA_EB						( BIT(1) )
#define BIT_DMA_CKG_EN						( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_AHB_CTRL2 */
#define BITS_BUSMON1_CHN_SEL(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_AXIBUSMON_CHN_SEL(_X_)				( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_BUSMON0_CHN_SEL(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_BUSMON1_EB						( BIT(3) )
#define BIT_SPINLOCK_EB						( BIT(2) )
#define BIT_AXIBUSMON_EB					( BIT(1) )
#define BIT_BUSMON0_EB						( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_AHB_RST0_STS */
#define BIT_TFT_SOFT_RST					( BIT(11) )
#define BIT_PPP1_SOFT_RST					( BIT(10) )
#define BIT_PPP0_SOFT_RST					( BIT(9) )
#define BIT_SEC1_CMD_PARSER_RST					( BIT(8) )
#define BIT_SEC0_CMD_PARSER_RST					( BIT(7) )
#define BIT_SPINLOCK_SOFT_RST					( BIT(6) )
#define BIT_SEC1_SOFT_RST					( BIT(5) )
#define BIT_SEC0_SOFT_RST					( BIT(4) )
#define BIT_BUSMON1_SOFT_RST					( BIT(3) )
#define BIT_AXIBUSMON_SOFT_RST					( BIT(2) )
#define BIT_BUSMON0_SOFT_RST					( BIT(1) )
#define BIT_DMA_SOFT_RST					( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_CA5_RST_SET */
#define BIT_CA5_MTX_SOFT_RST					( BIT(3) )
#define BIT_CA5_L2_SOFT_RST					( BIT(2) )
#define BIT_CA5_DBG_SOFT_RST					( BIT(1) )
#define BIT_CA5_CORE_SOFT_RST					( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_ARCH_EB */
#define BIT_AHB_ARCH_EB						( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_PTEST_FUNC */
#define BIT_PTEST_FUNC_ATSPEED_SEL				( BIT(2) )
#define BIT_PTEST_FUNC_MODE					( BIT(1) )

/* bits definitions for register REG_CORTEX_AHB_RF_AHB_MISC_CTL */
#define BITS_DMA_W_PROT(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DMA_R_PROT(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_LCP2PUB_ACCESS_EN					( BIT(4) )
#define BITS_MCU_SHM_CTRL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_AHB_RF_CA5_CTRL */
#define BIT_CA5_WDRESET_EN					( BIT(18) )
#define BIT_CA5_TS_EN						( BIT(17) )
#define BIT_CA5_CFGSDISABLE					( BIT(13) )
#define BITS_CA5_CLK_AXI_DIV(_X_)				( (_X_) << 11 & (BIT(11)|BIT(12)) )
#define BIT_CA5_CLK_DBG_EN_SEL					( BIT(10) )
#define BIT_CA5_CLK_DBG_EN					( BIT(9) )
#define BIT_CA5_DBGEN						( BIT(8) )
#define BIT_CA5_NIDEN						( BIT(7) )
#define BIT_CA5_SPIDEN						( BIT(6) )
#define BIT_CA5_SPNIDEN						( BIT(5) )
#define BIT_CA5_CP15DISABLE					( BIT(4) )
#define BIT_CA5_TEINIT						( BIT(3) )
#define BIT_CA5_L1RSTDISABLE					( BIT(2) )
#define BIT_CA5_L2CFGEND					( BIT(1) )
#define BIT_CA5_L2SPNIDEN					( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_AUTO_GATE_CTRL */
#define BIT_CORTEX_AHB_AUTO_GATE_EN				( BIT(5) )
#define BIT_CA5_DBG_FORCE_SLEEP					( BIT(4) )
#define BIT_CA5_DBG_AUTO_GATE_EN				( BIT(3) )
#define BIT_CA5_CORE_AUTO_GATE_EN				( BIT(2) )
#define BIT_LCPX_AUTO_GATE_EN					( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_AHB_PAUSE */
#define BIT_EMC_LIGHT_SLEEP_EN					( BIT(25) )
#define BIT_EMC_DEEP_SLEEP_EN					( BIT(24) )
#define BIT_APB_SLEEP_EN					( BIT(18) )
#define BIT_APB_PERI_FRC_SLP					( BIT(17) )
#define BIT_APB_PERI_FRC_ON					( BIT(16) )
#define BIT_MCU_FORCE_DEEP_SLEEP				( BIT(15) )
#define BIT_DMA_LSLP_EN						( BIT(4) )
#define BIT_MCU_LIGHT_SLEEP_EN					( BIT(3) )
#define BIT_MCU_DEEP_SLEEP_EN					( BIT(2) )
#define BIT_MCU_SYS_SLEEP_EN					( BIT(1) )
#define BIT_MCU_CORE_SLEEP					( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_AHB_SLP_CTL */
#define BIT_ARM_DAHB_SLEEP_EN					( BIT(4) )
#define BIT_MCUMTX_AUTO_GATE_EN					( BIT(3) )
#define BIT_MCU_AUTO_GATE_EN					( BIT(2) )
#define BIT_AHB_AUTO_GATE_EN					( BIT(1) )
#define BIT_ARM_AUTO_GATE_EN					( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_AHB_SLP_STS */
#define BIT_CHIP_SLP_ARM_CLR					( BIT(5) )
#define BIT_CHIP_SLEEP_REC_ARM					( BIT(4) )
#define BIT_APB_PERI_SLEEP					( BIT(3) )
#define BIT_DMA_BUSY						( BIT(1) )
#define BIT_EMC_SLEEP						( BIT(0) )

/* bits definitions for register REG_CORTEX_AHB_RF_MCU_CLK_CTL */
#define BIT_CLK_LCP2LDSP_BRG_EN					( BIT(24) )
#define BIT_CLK_LCP2PUB_BRG_EN					( BIT(23) )
#define BIT_CLK_LCP2CP0_BRG_EN					( BIT(22) )
#define BIT_CLK_CP_DBG_SEL					( BIT(21) )
#define BIT_CLK_MCU_DBG_SEL					( BIT(20) )
#define BITS_CLK_COM_DBG_SEL(_X_)				( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_CLK_SYS_DBG_SEL(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_CLK_AHB_DIV(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)) )
#define BITS_CLK_MCU_DIV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_CLK_APB_SEL(_X_)					( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_CLK_MCU_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_AHB_RF_RES_REG0 */
#define BITS_RES_REG0(_X_)					(_X_)

/* bits definitions for register REG_CORTEX_AHB_RF_CHIP_ID */
#define BITS_CHIP_ID(_X_)					(_X_)

#endif
