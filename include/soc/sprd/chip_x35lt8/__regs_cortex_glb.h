/* the head file modifier:     g   2015-03-19 15:43:07*/

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


#ifndef __H_REGS_CORTEX_GLB_HEADFILE_H__
#define __H_REGS_CORTEX_GLB_HEADFILE_H__ __FILE__

#define  REGS_CORTEX_GLB

/* registers definitions for CORTEX_GLB */
#define REG_CORTEX_GLB_APB_EB0_STS			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0008)/*APB_EB0_STS*/
#define REG_CORTEX_GLB_APB_RST0_STS			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0014)/*APB_RST0_STS*/
#define REG_CORTEX_GLB_APB_MCU_RST			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0018)/*APB_MCU_RST*/
#define REG_CORTEX_GLB_APB_CLK_SEL0			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x001C)/*APB_CLK_SEL0*/
#define REG_CORTEX_GLB_APB_CLK_SEL1			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0020)/*APB_CLK_SEL1*/
#define REG_CORTEX_GLB_APB_ARCH_EB			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0024)/*APB_ARCH_EB*/
#define REG_CORTEX_GLB_APB_MISC_CTL0			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0028)/*APB_MISC_CTL0*/
#define REG_CORTEX_GLB_APB_MISC_CTL1			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x002C)/*APB_MISC_CTL1*/
#define REG_CORTEX_GLB_APB_CLK_SEL2			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0030)/*APB_CLK_SEL2*/
#define REG_CORTEX_GLB_APB_BUS_CTL0			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0058)/*APB_BUS_CTL0*/
#define REG_CORTEX_GLB_APB_DSP_INT_CLR			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0074)/*APB_DSP_INT_CLR*/
#define REG_CORTEX_GLB_APB_MISC_INT_STS			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0078)/*APB_MISC_INT_STS*/
#define REG_CORTEX_GLB_APB_HWRST			SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0080)/*APB_HWRST*/
#define REG_CORTEX_GLB_APB_ARM_BOOT_ADDR		SCI_ADDR(REGS_CORTEX_GLB_BASE, 0x0084)/*APB_ARM_BOOT_ADDR*/



/* bits definitions for register REG_CORTEX_GLB_APB_EB0_STS */
#define BIT_EIC_RTC_EB						( BIT(19) )
#define BIT_EIC_EB						( BIT(18) )
#define BIT_RFFE_EB						( BIT(17) )
#define BIT_EPT_EB						( BIT(16) )
#define BIT_GPIO_EB						( BIT(15) )
#define BIT_TMR_RTC_EB						( BIT(14) )
#define BIT_TMR_EB						( BIT(13) )
#define BIT_SYSTMR_RTC_EB					( BIT(12) )
#define BIT_SYSTMR_EB						( BIT(11) )
#define BIT_IIS3_EB						( BIT(10) )
#define BIT_IIS2_EB						( BIT(9) )
#define BIT_IIS1_EB						( BIT(8) )
#define BIT_IIS0_EB						( BIT(7) )
#define BIT_SIM2_EB						( BIT(6) )
#define BIT_SIM1_EB						( BIT(5) )
#define BIT_SIM0_EB						( BIT(4) )
#define BIT_UART1_EB						( BIT(3) )
#define BIT_UART0_EB						( BIT(2) )
#define BIT_WDG_RTC_EB						( BIT(1) )
#define BIT_WDG_EB						( BIT(0) )

/* bits definitions for register REG_CORTEX_GLB_APB_RST0_STS */
#define BIT_EIC_SOFT_RST					( BIT(16) )
#define BIT_RFFE_SOFT_RST					( BIT(15) )
#define BIT_MCU_DSP_RST						( BIT(14) )
#define BIT_EPT_SOFT_RST					( BIT(13) )
#define BIT_GPIO_SOFT_RST					( BIT(12) )
#define BIT_TMR_SOFT_RST					( BIT(11) )
#define BIT_SYSTMR_SOFT_RST					( BIT(10) )
#define BIT_IIS3_SOFT_RST					( BIT(9) )
#define BIT_IIS2_SOFT_RST					( BIT(8) )
#define BIT_IIS1_SOFT_RST					( BIT(7) )
#define BIT_IIS0_SOFT_RST					( BIT(6) )
#define BIT_SIM2_SOFT_RST					( BIT(5) )
#define BIT_SIM1_SOFT_RST					( BIT(4) )
#define BIT_SIM0_SOFT_RST					( BIT(3) )
#define BIT_UART1_SOFT_RST					( BIT(2) )
#define BIT_UART0_SOFT_RST					( BIT(1) )
#define BIT_WDG_SOFT_RST					( BIT(0) )

/* bits definitions for register REG_CORTEX_GLB_APB_MCU_RST */
#define BIT_MCU_SOFT_RST_SET					( BIT(0) )

/* bits definitions for register REG_CORTEX_GLB_APB_CLK_SEL0 */
#define BITS_CLK_UART1_DIV(_X_)					( (_X_) << 7 & (BIT(7)|BIT(8)|BIT(9)) )
#define BITS_CLK_UART1_SEL(_X_)					( (_X_) << 5 & (BIT(5)|BIT(6)) )
#define BITS_CLK_UART0_DIV(_X_)					( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)) )
#define BITS_CLK_UART0_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_GLB_APB_CLK_SEL1 */
#define BIT_CLK_IIS1_PAD_SEL					( BIT(24) )
#define BITS_CLK_IIS1_DIV(_X_)					( (_X_) << 18 & (BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_CLK_IIS1_SEL(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BIT_CLK_IIS0_PAD_SEL					( BIT(8) )
#define BITS_CLK_IIS0_DIV(_X_)					( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CLK_IIS0_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_GLB_APB_ARCH_EB */
#define BIT_RTC_ARCH_EB						( BIT(1) )
#define BIT_APB_ARCH_EB						( BIT(0) )

/* bits definitions for register REG_CORTEX_GLB_APB_MISC_CTL0 */
#define BIT_ALL_CLK_EN						( BIT(28) )
#define BIT_DMA_LSLP_EN						( BIT(27) )
#define BIT_WAKEUP_XTL_EN_3G					( BIT(26) )
#define BIT_WAKEUP_XTL_EN_2G					( BIT(25) )
#define BIT_ARM_JTAG_EN						( BIT(24) )
#define BITS_ARM_FRC_STOP(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_CORTEX_GLB_APB_MISC_CTL1 */
#define BITS_BUFON_CTRL(_X_)					( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BIT_SIM2_CLK_POLARITY					( BIT(3) )
#define BIT_SIM1_CLK_POLARITY					( BIT(2) )
#define BIT_SIM0_CLK_POLARITY					( BIT(1) )
#define BIT_ROM_CLK_EN						( BIT(0) )

/* bits definitions for register REG_CORTEX_GLB_APB_CLK_SEL2 */
#define BIT_CLK_IIS3_PAD_SEL					( BIT(24) )
#define BITS_CLK_IIS3_DIV(_X_)					( (_X_) << 18 & (BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_CLK_IIS3_SEL(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BIT_CLK_IIS2_PAD_SEL					( BIT(8) )
#define BITS_CLK_IIS2_DIV(_X_)					( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CLK_IIS2_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CORTEX_GLB_APB_BUS_CTL0 */
#define BIT_IIS3_CTRL_SEL					( BIT(7) )
#define BIT_IIS2_CTRL_SEL					( BIT(6) )
#define BIT_RFFE_CTRL_SEL					( BIT(5) )
#define BIT_IIS1_CTRL_SEL					( BIT(3) )
#define BIT_IIS0_CTRL_SEL					( BIT(2) )
#define BIT_UART0_CTRL_SEL					( BIT(1) )
#define BIT_UART1_CTRL_SEL					( BIT(0) )

/* bits definitions for register REG_CORTEX_GLB_APB_DSP_INT_CLR */
#define BIT_RFT_INT_CLR						( BIT(2) )

/* bits definitions for register REG_CORTEX_GLB_APB_MISC_INT_STS */
#define BIT_RFT_INT						( BIT(2) )

/* bits definitions for register REG_CORTEX_GLB_APB_HWRST */
#define BITS_HWRST_REG(_X_)					(_X_)

/* bits definitions for register REG_CORTEX_GLB_APB_ARM_BOOT_ADDR */
#define BITS_ARMBOOT_ADDR(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

#endif
