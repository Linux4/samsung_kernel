/* the head file modifier:     ang   2015-01-12 21:07:29*/

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


#ifndef __H_REGS_AON_APB_HEADFILE_H__
#define __H_REGS_AON_APB_HEADFILE_H__ __FILE__

#define  REGS_AON_APB

/* registers definitions for AON_APB */
#define REG_AON_APB_APB_EB0				SCI_ADDR(REGS_AON_APB_BASE, 0x0000)/*AHB_EB0*/
#define REG_AON_APB_APB_EB1				SCI_ADDR(REGS_AON_APB_BASE, 0x0004)/*AHB_EB1*/
#define REG_AON_APB_APB_RST0				SCI_ADDR(REGS_AON_APB_BASE, 0x0008)/*AHB_RST0*/
#define REG_AON_APB_APB_RST1				SCI_ADDR(REGS_AON_APB_BASE, 0x000C)/*AHB_RST1*/
#define REG_AON_APB_APB_RTC_EB				SCI_ADDR(REGS_AON_APB_BASE, 0x0010)/*APB_RTC_EB*/
#define REG_AON_APB_REC_26MHZ_BUF_CFG			SCI_ADDR(REGS_AON_APB_BASE, 0x0014)/*REC_26MHZ_BUF_CFG*/
#define REG_AON_APB_SINDRV_CTRL				SCI_ADDR(REGS_AON_APB_BASE, 0x0018)/*SINDRV_CTRL*/
#define REG_AON_APB_ADA_SEL_CTRL			SCI_ADDR(REGS_AON_APB_BASE, 0x001C)/*ADA_SEL_CTRL*/
#define REG_AON_APB_VBC_CTRL				SCI_ADDR(REGS_AON_APB_BASE, 0x0020)/*VBC_CTRL*/
#define REG_AON_APB_PWR_CTRL				SCI_ADDR(REGS_AON_APB_BASE, 0x0024)/*PWR_CTRL*/
#define REG_AON_APB_TS_CFG				SCI_ADDR(REGS_AON_APB_BASE, 0x0028)/*TS_CFG*/
#define REG_AON_APB_BOOT_MODE				SCI_ADDR(REGS_AON_APB_BASE, 0x002C)/*BOOT_MODE*/
#define REG_AON_APB_BB_BG_CTRL				SCI_ADDR(REGS_AON_APB_BASE, 0x0030)/*BB_BG_CTRL*/
#define REG_AON_APB_CP_ARM_JTAG_CTRL			SCI_ADDR(REGS_AON_APB_BASE, 0x0034)/*CP_ARM_JTAG_CTRL*/
#define REG_AON_APB_PLL_SOFT_CNT_DONE			SCI_ADDR(REGS_AON_APB_BASE, 0x0038)/*PLL_SOFT_CNT_DONE*/
#define REG_AON_APB_DCXO_LC_REG0			SCI_ADDR(REGS_AON_APB_BASE, 0x003C)/*DCXO_LC_REG0*/
#define REG_AON_APB_DCXO_LC_REG1			SCI_ADDR(REGS_AON_APB_BASE, 0x0040)/*DCXO_LC_REG1*/
#define REG_AON_APB_DSI_PHY_CTRL			SCI_ADDR(REGS_AON_APB_BASE, 0x0048)/*DSI_PHY_CTRL*/
#define REG_AON_APB_CSI0_PHY_CTRL			SCI_ADDR(REGS_AON_APB_BASE, 0x004C)/*CSI0_PHY_CTRL*/
#define REG_AON_APB_CSI1_PHY_CTRL			SCI_ADDR(REGS_AON_APB_BASE, 0x0050)/*CSI1_PHY_CTRL*/
#define REG_AON_APB_FUNC_TEST_BOOT_ADDR			SCI_ADDR(REGS_AON_APB_BASE, 0x0054)/*FUNC_TEST_BOOT_ADDR*/
#define REG_AON_APB_RINGOSC_CTRL			SCI_ADDR(REGS_AON_APB_BASE, 0x0058)/*RINGOSC_CTRL*/
#define REG_AON_APB_RINGOSC_OBS_CNT			SCI_ADDR(REGS_AON_APB_BASE, 0x005C)/*RINGOSC_OBS_CNT*/
#define REG_AON_APB_DDR_ZQ_CONTROL			SCI_ADDR(REGS_AON_APB_BASE, 0x0060)/*DDR_ZQ_CONTROL*/
#define REG_AON_APB_WBT_BG_CTRL				SCI_ADDR(REGS_AON_APB_BASE, 0x0064)/*WBT_BG_CTRL*/
#define REG_AON_APB_CLK_DMC_CFG				SCI_ADDR(REGS_AON_APB_BASE, 0x0080)/*CLK_DMC_CFG*/
#define REG_AON_APB_SOFT_DFS_CTRL			SCI_ADDR(REGS_AON_APB_BASE, 0x0084)/*SOFT_DFS_CTRL*/
#define REG_AON_APB_CLK26M_CFG				SCI_ADDR(REGS_AON_APB_BASE, 0x0088)/*CLK26M_CFG*/
#define REG_AON_APB_HARD_DFS_CTRL_LO			SCI_ADDR(REGS_AON_APB_BASE, 0x0090)/*HARD_DFS_CTRL_LO*/
#define REG_AON_APB_HARD_DFS_CTRL_HI			SCI_ADDR(REGS_AON_APB_BASE, 0x0094)/*HARD_DFS_CTRL_HI*/
#define REG_AON_APB_AON_CHIP_ID				SCI_ADDR(REGS_AON_APB_BASE, 0x00FC)/*AON_CHIP_ID*/
#define REG_AON_APB_MPLL_CFG				SCI_ADDR(REGS_AON_APB_BASE, 0x3000)/*MPLL_CFG*/
#define REG_AON_APB_DPLL_CFG				SCI_ADDR(REGS_AON_APB_BASE, 0x3004)/*DPLL_CFG*/
#define REG_AON_APB_TDPLL_CFG				SCI_ADDR(REGS_AON_APB_BASE, 0x3008)/*TDPLL_CFG*/
#define REG_AON_APB_CPLL_CFG				SCI_ADDR(REGS_AON_APB_BASE, 0x300C)/*CPLL_CFG*/
#define REG_AON_APB_WIFIPLL0_CFG			SCI_ADDR(REGS_AON_APB_BASE, 0x3010)/*WIFIPLL0_CFG*/
#define REG_AON_APB_WIFIPLL1_CFG			SCI_ADDR(REGS_AON_APB_BASE, 0x3014)/*WIFIPLL1_CFG*/
#define REG_AON_APB_WPLL_CFG0				SCI_ADDR(REGS_AON_APB_BASE, 0x3018)/*WPLL_CFG0*/
#define REG_AON_APB_WPLL_CFG1				SCI_ADDR(REGS_AON_APB_BASE, 0x301C)/*WPLL_CFG1*/
#define REG_AON_APB_AON_CGM_CFG				SCI_ADDR(REGS_AON_APB_BASE, 0x3020)/*AON_CGM_CFG*/
#define REG_AON_APB_CP0_ADDR_REMAP_CTRL0		SCI_ADDR(REGS_AON_APB_BASE, 0x3024)/*CP0_ADDR_REMAP_CTRL0*/
#define REG_AON_APB_CP0_ADDR_REMAP_CTRL1		SCI_ADDR(REGS_AON_APB_BASE, 0x3028)/*CP0_ADDR_REMAP_CTRL1*/
#define REG_AON_APB_CP1_ADDR_REMAP_CTRL0		SCI_ADDR(REGS_AON_APB_BASE, 0x302C)/*CP1_ADDR_REMAP_CTRL0*/
#define REG_AON_APB_CP1_ADDR_REMAP_CTRL1		SCI_ADDR(REGS_AON_APB_BASE, 0x3030)/*CP1_ADDR_REMAP_CTRL1*/
#define REG_AON_APB_CP2_ADDR_REMAP_CTRL0		SCI_ADDR(REGS_AON_APB_BASE, 0x3034)/*CP2_ADDR_REMAP_CTRL0*/
#define REG_AON_APB_CP2_ADDR_REMAP_CTRL1		SCI_ADDR(REGS_AON_APB_BASE, 0x3038)/*CP2_ADDR_REMAP_CTRL1*/
#define REG_AON_APB_IO_DLY_CTRL				SCI_ADDR(REGS_AON_APB_BASE, 0x303C)/*IO_DLY_CTRL*/
#define REG_AON_APB_AP_WPROT_EN				SCI_ADDR(REGS_AON_APB_BASE, 0x3040)/*AP_WPROT_EN*/
#define REG_AON_APB_CP0_WPROT_EN			SCI_ADDR(REGS_AON_APB_BASE, 0x3044)/*CP0_WPROT_EN*/
#define REG_AON_APB_CP1_WPROT_EN			SCI_ADDR(REGS_AON_APB_BASE, 0x3048)/*CP1_WPROT_EN*/
#define REG_AON_APB_CP2_WPROT_EN			SCI_ADDR(REGS_AON_APB_BASE, 0x304C)/*CP2_WPROT_EN*/
#define REG_AON_APB_PMU_RST_MONITOR			SCI_ADDR(REGS_AON_APB_BASE, 0x3050)/*PMU_RST_MONITOR*/
#define REG_AON_APB_THM_RST_MONITOR			SCI_ADDR(REGS_AON_APB_BASE, 0x3054)/*THM_RST_MONITOR*/
#define REG_AON_APB_AP_RST_MONITOR			SCI_ADDR(REGS_AON_APB_BASE, 0x3058)/*AP_RST_MONITOR*/
#define REG_AON_APB_CA7_RST_MONITOR			SCI_ADDR(REGS_AON_APB_BASE, 0x305C)/*CA7_RST_MONITOR*/
#define REG_AON_APB_BOND_OPT0				SCI_ADDR(REGS_AON_APB_BASE, 0x3060)/*BOND_OPT0*/
#define REG_AON_APB_BOND_OPT1				SCI_ADDR(REGS_AON_APB_BASE, 0x3064)/*BOND_OPT1*/
#define REG_AON_APB_RES_REG0				SCI_ADDR(REGS_AON_APB_BASE, 0x3068)/*RES_REG0*/
#define REG_AON_APB_RES_REG1				SCI_ADDR(REGS_AON_APB_BASE, 0x306C)/*RES_REG1*/
#define REG_AON_APB_MPLL_CFG1				SCI_ADDR(REGS_AON_APB_BASE, 0x3070)/*MPLL_CFG1*/
#define REG_AON_APB_DPLL_CFG1				SCI_ADDR(REGS_AON_APB_BASE, 0x3074)/*DPLL_CFG1*/
#define REG_AON_APB_TDPLL_CFG1				SCI_ADDR(REGS_AON_APB_BASE, 0x3078)/*TDPLL_CFG1*/
#define REG_AON_APB_CPLL_CFG1				SCI_ADDR(REGS_AON_APB_BASE, 0x307C)/*CPLL_CFG1*/
#define REG_AON_APB_WIFIPLL1_CFG1			SCI_ADDR(REGS_AON_APB_BASE, 0x3080)/*WIFIPLL1_CFG1*/
#define REG_AON_APB_WIFIPLL2_CFG1			SCI_ADDR(REGS_AON_APB_BASE, 0x3084)/*WIFIPLL2_CFG1*/
#define REG_AON_APB_AON_QOS_CFG				SCI_ADDR(REGS_AON_APB_BASE, 0x3088)/*AON_QOS_CFG*/
#define REG_AON_APB_BB_LDO_CAL_START			SCI_ADDR(REGS_AON_APB_BASE, 0x308C)/*BB_LDO_CAL_START*/
#define REG_AON_APB_WBT_BG_LDO				SCI_ADDR(REGS_AON_APB_BASE, 0x3090)/*WBT_BG_LDO*/
#define REG_AON_APB_ANA_BB_MISC				SCI_ADDR(REGS_AON_APB_BASE, 0x3094)/*ANA_BB_MISC*/
#define REG_AON_APB_DJTAG_MUX_SEL			SCI_ADDR(REGS_AON_APB_BASE, 0x3098)/*DJTAG_MUX_SEL*/
#define REG_AON_APB_BBPLL_RESERVED_0			SCI_ADDR(REGS_AON_APB_BASE, 0x309C)/*BBPLL_RESERVED_0*/
#define REG_AON_APB_BBPLL_RESERVED_1			SCI_ADDR(REGS_AON_APB_BASE, 0x30A0)/*BBPLL_RESERVED_1*/
#define REG_AON_APB_RPLL_CFG0				SCI_ADDR(REGS_AON_APB_BASE, 0x30A4)/*RPLL_CFG0*/
#define REG_AON_APB_AON_DEBUG_CFG			SCI_ADDR(REGS_AON_APB_BASE, 0x30A8)/*AON_DEBUG_CFG*/



/* bits definitions for register REG_AON_APB_APB_EB0 */
#define BIT_I2C_EB						( BIT(31) )
#define BIT_CA7_DAP_EB						( BIT(30) )
#define BIT_CA7_TS1_EB						( BIT(29) )
#define BIT_CA7_TS0_EB						( BIT(28) )
#define BIT_GPU_EB						( BIT(27) )
#define BIT_AON_CKG_EB                                        ( BIT(26) )
#define BIT_MM_EB						( BIT(25) )
#define BIT_AP_WDG_EB						( BIT(24) )
#define BIT_MSPI_EB						( BIT(23) )
#define BIT_SPLK_EB						( BIT(22) )
#define BIT_IPI_EB						( BIT(21) )
#define BIT_PIN_EB						( BIT(20) )
#define BIT_VBC_EB						( BIT(19) )
#define BIT_AUD_EB						( BIT(18) )
#define BIT_AUDIF_EB						( BIT(17) )
#define BIT_ADI_EB						( BIT(16) )
#define BIT_INTC_EB						( BIT(15) )
#define BIT_EIC_EB						( BIT(14) )
#define BIT_EFUSE_EB						( BIT(13) )
#define BIT_AP_TMR0_EB						( BIT(12) )
#define BIT_AON_TMR_EB						( BIT(11) )
#define BIT_AP_SYST_EB						( BIT(10) )
#define BIT_AON_SYST_EB						( BIT(9) )
#define BIT_KPD_EB						( BIT(8) )
#define BIT_PWM3_EB						( BIT(7) )
#define BIT_PWM2_EB						( BIT(6) )
#define BIT_PWM1_EB						( BIT(5) )
#define BIT_PWM0_EB						( BIT(4) )
#define BIT_GPIO_EB						( BIT(3) )
#define BIT_TPC_EB						( BIT(2) )
#define BIT_FM_EB						( BIT(1) )
#define BIT_ADC_EB						( BIT(0) )

/* bits definitions for register REG_AON_APB_APB_EB1 */
#define BIT_EMC_REF_CKG_EN					( BIT(20) )
#define BIT_DJTAG_EB						( BIT(19) )
#define BIT_RINGOSC_EB						( BIT(18) )
#define BIT_PUB_REG_EB						( BIT(17) )
#define BIT_DMC_EB						( BIT(16) )
#define BIT_RFTI_SBI_EB						( BIT(15) )
#define BIT_MDAR_EB						( BIT(14) )
#define BIT_GSP_EMC_EB						( BIT(13) )
#define BIT_ZIP_EMC_EB						( BIT(12) )
#define BIT_DISP_EMC_EB						( BIT(11) )
#define BIT_AP_TMR2_EB						( BIT(10) )
#define BIT_AP_TMR1_EB						( BIT(9) )
#define BIT_CA7_WDG_EB						( BIT(8) )
#define BIT_AVS1_EB						( BIT(7) )
#define BIT_AVS0_EB						( BIT(6) )
#define BIT_PROBE_EB						( BIT(5) )
#define BIT_AUX2_EB						( BIT(4) )
#define BIT_AUX1_EB						( BIT(3) )
#define BIT_AUX0_EB						( BIT(2) )
#define BIT_THM_EB						( BIT(1) )
#define BIT_PMU_EB						( BIT(0) )

/* bits definitions for register REG_AON_APB_APB_RST0 */
#define BIT_I2C_SOFT_RST					( BIT(30) )
#define BIT_CA7_TS1_SOFT_RST					( BIT(29) )
#define BIT_CA7_TS0_SOFT_RST					( BIT(28) )
#define BIT_DAP_MTX_SOFT_RST					( BIT(27) )
#define BIT_MSPI1_SOFT_RST					( BIT(26) )
#define BIT_MSPI0_SOFT_RST					( BIT(25) )
#define BIT_SPLK_SOFT_RST					( BIT(24) )
#define BIT_IPI_SOFT_RST					( BIT(23) )
#define BIT_AON_CKG_SOFT_RST                                  ( BIT(22) )
#define BIT_PIN_SOFT_RST					( BIT(21) )
#define BIT_VBC_SOFT_RST					( BIT(20) )
#define BIT_AUD_SOFT_RST					( BIT(19) )
#define BIT_AUDIF_SOFT_RST					( BIT(18) )
#define BIT_ADI_SOFT_RST					( BIT(17) )
#define BIT_INTC_SOFT_RST					( BIT(16) )
#define BIT_EIC_SOFT_RST					( BIT(15) )
#define BIT_EFUSE_SOFT_RST					( BIT(14) )
#define BIT_AP_WDG_SOFT_RST					( BIT(13) )
#define BIT_AP_TMR0_SOFT_RST					( BIT(12) )
#define BIT_AON_TMR_SOFT_RST					( BIT(11) )
#define BIT_AP_SYST_SOFT_RST					( BIT(10) )
#define BIT_AON_SYST_SOFT_RST					( BIT(9) )
#define BIT_KPD_SOFT_RST					( BIT(8) )
#define BIT_PWM3_SOFT_RST					( BIT(7) )
#define BIT_PWM2_SOFT_RST					( BIT(6) )
#define BIT_PWM1_SOFT_RST					( BIT(5) )
#define BIT_PWM0_SOFT_RST					( BIT(4) )
#define BIT_GPIO_SOFT_RST					( BIT(3) )
#define BIT_TPC_SOFT_RST					( BIT(2) )
#define BIT_FM_SOFT_RST						( BIT(1) )
#define BIT_ADC_SOFT_RST					( BIT(0) )

/* bits definitions for register REG_AON_APB_APB_RST1 */
#define BIT_PUB_DJTAG_SOFT_RST					( BIT(23) )
#define BIT_GPU_DJTAG_SOFT_RST					( BIT(22) )
#define BIT_MM_DJTAG_SOFT_RST					( BIT(21) )
#define BIT_CP2_DJTAG_SOFT_RST					( BIT(20) )
#define BIT_CP0_DJTAG_SOFT_RST					( BIT(19) )
#define BIT_AP_DJTAG_SOFT_RST					( BIT(18) )
#define BIT_AON_DJTAG_SOFT_RST					( BIT(17) )
#define BIT_LVDSRF_CALI_SOFT_RST				( BIT(16) )
#define BIT_LTH_SOFT_RST					( BIT(15) )
#define BIT_RINGOSC_SOFT_RST					( BIT(14) )
#define BIT_RFTI_SBI_SOFT_RST					( BIT(13) )
#define BIT_MDAR_SOFT_RST					( BIT(12) )
#define BIT_BB_CAL_SOFT_RST					( BIT(11) )
#define BIT_DCXO_LC_SOFT_RST					( BIT(10) )
#define BIT_AP_TMR2_SOFT_RST					( BIT(9) )
#define BIT_AP_TMR1_SOFT_RST					( BIT(8) )
#define BIT_CA7_WDG_SOFT_RST					( BIT(7) )
#define BIT_AVS1_SOFT_RST					( BIT(6) )
#define BIT_AVS0_SOFT_RST					( BIT(5) )
#define BIT_DMC_PHY_SOFT_RST					( BIT(4) )
#define BIT_GPU_THMA_SOFT_RST					( BIT(3) )
#define BIT_ARM_THMA_SOFT_RST					( BIT(2) )
#define BIT_THM_SOFT_RST					( BIT(1) )
#define BIT_PMU_SOFT_RST					( BIT(0) )

/* bits definitions for register REG_AON_APB_APB_RTC_EB */
#define BIT_BB_CAL_RTC_EB					( BIT(18) )
#define BIT_DCXO_LC_RTC_EB					( BIT(17) )
#define BIT_AP_TMR2_RTC_EB					( BIT(16) )
#define BIT_AP_TMR1_RTC_EB					( BIT(15) )
#define BIT_GPU_THMA_RTC_AUTO_EN				( BIT(14) )
#define BIT_ARM_THMA_RTC_AUTO_EN				( BIT(13) )
#define BIT_GPU_THMA_RTC_EB					( BIT(12) )
#define BIT_ARM_THMA_RTC_EB					( BIT(11) )
#define BIT_THM_RTC_EB						( BIT(10) )
#define BIT_CA7_WDG_RTC_EB					( BIT(9) )
#define BIT_AP_WDG_RTC_EB					( BIT(8) )
#define BIT_EIC_RTCDV5_EB					( BIT(7) )
#define BIT_EIC_RTC_EB						( BIT(6) )
#define BIT_AP_TMR0_RTC_EB					( BIT(5) )
#define BIT_AON_TMR_RTC_EB					( BIT(4) )
#define BIT_AP_SYST_RTC_EB					( BIT(3) )
#define BIT_AON_SYST_RTC_EB					( BIT(2) )
#define BIT_KPD_RTC_EB						( BIT(1) )
#define BIT_ARCH_RTC_EB						( BIT(0) )

/* bits definitions for register REG_AON_APB_REC_26MHZ_BUF_CFG */
#define BITS_PLL_PROBE_SEL(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_REC_26MHZ_1_FORCE_PD				( BIT(5) )
#define BIT_REC_26MHZ_1_CUR_SEL					( BIT(4) )
#define BIT_REC_26MHZ_0_CUR_SEL					( BIT(0) )

/* bits definitions for register REG_AON_APB_SINDRV_CTRL */
#define BITS_SINDRV_LVL(_X_)					( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BIT_SINDRV_CLIP_MODE					( BIT(2) )
#define BIT_SINDRV_ENA_SQUARE					( BIT(1) )
#define BIT_SINDRV_ENA						( BIT(0) )

/* bits definitions for register REG_AON_APB_ADA_SEL_CTRL */
#define BIT_RAM_TD_MODE_SEL					( BIT(4) )
#define BIT_TW_MODE_SEL						( BIT(3) )
#define BIT_WGADC_DIV_EN					( BIT(2) )
#define BIT_AFCDAC_SYS_SEL					( BIT(1) )
#define BIT_APCDAC_SYS_SEL					( BIT(0) )

/* bits definitions for register REG_AON_APB_VBC_CTRL */
#define BIT_AUDIF_CKG_AUTO_EN					( BIT(20) )
#define BITS_AUD_INT_SYS_SEL(_X_)				( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_VBC_AFIFO_INT_SYS_SEL(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_VBC_AD23_INT_SYS_SEL(_X_)				( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_VBC_AD01_INT_SYS_SEL(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_VBC_DA01_INT_SYS_SEL(_X_)				( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_VBC_AD23_DMA_SYS_SEL(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_VBC_AD01_DMA_SYS_SEL(_X_)				( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_VBC_DA01_DMA_SYS_SEL(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_VBC_INT_CP0_ARM_SEL					( BIT(3) )
#define BIT_VBC_INT_CP1_ARM_SEL					( BIT(2) )
#define BIT_VBC_DMA_CP0_ARM_SEL					( BIT(1) )
#define BIT_VBC_DMA_CP1_ARM_SEL					( BIT(0) )

/* bits definitions for register REG_AON_APB_PWR_CTRL */
#define BIT_DSI_PHY_PD_S					( BIT(15) )
#define BIT_CSI1_PHY_PD_S					( BIT(14) )
#define BIT_CSI0_PHY_PD_S					( BIT(13) )
#define BIT_DSI_PHY_PD						( BIT(12) )
#define BIT_CSI1_PHY_PD						( BIT(11) )
#define BIT_CSI0_PHY_PD						( BIT(10) )
#define BIT_CA7_TS1_STOP					( BIT(9) )
#define BIT_CA7_TS0_STOP					( BIT(8) )
#define BIT_EFUSE_BIST_PWR_ON					( BIT(3) )
#define BIT_FORCE_DSI_PHY_SHUTDOWNZ				( BIT(2) )
#define BIT_FORCE_CSI_PHY_SHUTDOWNZ				( BIT(1) )
#define BIT_USB_PHY_PD						( BIT(0) )

/* bits definitions for register REG_AON_APB_TS_CFG */
#define BIT_CSYSACK_TS_LP_2					( BIT(13) )
#define BIT_CSYSREQ_TS_LP_2					( BIT(12) )
#define BIT_CSYSACK_TS_LP_1					( BIT(11) )
#define BIT_CSYSREQ_TS_LP_1					( BIT(10) )
#define BIT_CSYSACK_TS_LP_0					( BIT(9) )
#define BIT_CSYSREQ_TS_LP_0					( BIT(8) )
#define BIT_EVENTACK_RESTARTREQ_TS01				( BIT(4) )
#define BIT_EVENT_RESTARTREQ_TS01				( BIT(1) )
#define BIT_EVENT_HALTREQ_TS01					( BIT(0) )

/* bits definitions for register REG_AON_APB_BOOT_MODE */
#define BIT_WPLL_OVR_FREQ_SEL					( BIT(12) )
#define BIT_PTEST_FUNC_ATSPEED_SEL				( BIT(8) )
#define BIT_PTEST_FUNC_MODE					( BIT(7) )
#define BIT_PTEST_FUNC_DMA_MODE					( BIT(6) )
#define BIT_USB_DLOAD_EN					( BIT(4) )
#define BIT_ARM_BOOT_MD3					( BIT(3) )
#define BIT_ARM_BOOT_MD2					( BIT(2) )
#define BIT_ARM_BOOT_MD1					( BIT(1) )
#define BIT_ARM_BOOT_MD0					( BIT(0) )

/* bits definitions for register REG_AON_APB_BB_BG_CTRL */
#define BIT_BB_CON_BG						( BIT(22) )
#define BITS_BB_BG_RSV(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)) )
#define BITS_BB_LDO_V(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BIT_BB_BG_RBIAS_EN					( BIT(15) )
#define BIT_BB_BG_IEXT_IB_EN					( BIT(14) )
#define BITS_BB_LDO_REFCTRL(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BIT_BB_LDO_AUTO_PD_EN					( BIT(11) )
#define BIT_BB_LDO_SLP_PD_EN					( BIT(10) )
#define BIT_BB_LDO_FORCE_ON					( BIT(9) )
#define BIT_BB_LDO_FORCE_PD					( BIT(8) )
#define BIT_BB_BG_AUTO_PD_EN					( BIT(3) )
#define BIT_BB_BG_SLP_PD_EN					( BIT(2) )
#define BIT_BB_BG_FORCE_ON					( BIT(1) )
#define BIT_BB_BG_FORCE_PD					( BIT(0) )

/* bits definitions for register REG_AON_APB_CP_ARM_JTAG_CTRL */
#define BITS_CP_ARM_JTAG_PIN_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AON_APB_PLL_SOFT_CNT_DONE */
#define BIT_XTLBUF1_SOFT_CNT_DONE				( BIT(9) )
#define BIT_XTLBUF0_SOFT_CNT_DONE				( BIT(8) )
#define BIT_WIFIPLL2_SOFT_CNT_DONE				( BIT(6) )
#define BIT_WIFIPLL1_SOFT_CNT_DONE				( BIT(5) )
#define BIT_CPLL_SOFT_CNT_DONE					( BIT(4) )
#define BIT_WPLL_SOFT_CNT_DONE					( BIT(3) )
#define BIT_TDPLL_SOFT_CNT_DONE					( BIT(2) )
#define BIT_DPLL_SOFT_CNT_DONE					( BIT(1) )
#define BIT_MPLL_SOFT_CNT_DONE					( BIT(0) )

/* bits definitions for register REG_AON_APB_DCXO_LC_REG0 */
#define BIT_DCXO_LC_FLAG					( BIT(8) )
#define BIT_DCXO_LC_FLAG_CLR					( BIT(1) )
#define BIT_DCXO_LC_CNT_CLR					( BIT(0) )

/* bits definitions for register REG_AON_APB_DCXO_LC_REG1 */
#define BITS_DCXO_LC_CNT(_X_)					(_X_)

/* bits definitions for register REG_AON_APB_DSI_PHY_CTRL */
#define BIT_DSI_PHY_IF_SEL					( BIT(24) )
#define BITS_DSI_PHY_TRIMBG(_X_)				( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_DSI_PHY_TX_RCTL(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_DSI_PHY_RESERVED(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AON_APB_CSI0_PHY_CTRL */
#define BIT_CSI0_PHY_IF_SEL					( BIT(24) )
#define BITS_CSI0_PHY_RX_RCTL(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_CSI0_PHY_RESERVED(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AON_APB_CSI1_PHY_CTRL */
#define BIT_CSI1_PHY_IF_SEL					( BIT(24) )
#define BITS_CSI1_PHY_RX_RCTL(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_CSI1_PHY_RESERVED(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AON_APB_FUNC_TEST_BOOT_ADDR */
#define BITS_FUNC_TEST_BOOT_ADDR(_X_)				(_X_)

/* bits definitions for register REG_AON_APB_RINGOSC_CTRL */
#define BITS_RINGOSC_CNT_CLK_NUM(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)) )
#define BIT_AP_RINGOSC_EN_CA7_TOP				( BIT(15) )
#define BIT_AP_RINGOSC_EN_AP_TOP				( BIT(14) )
#define BITS_AP_RINGOSC_CTRL(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_RINGOSC_CTRL(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AON_APB_RINGOSC_OBS_CNT */
#define BIT_RINGOSC_OBS_CNT_OVERFLOW				( BIT(16) )
#define BITS_RINGOSC_OBS_CNT(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AON_APB_DDR_ZQ_CONTROL */
#define BIT_DDR_ZQ_CALOVER					( BIT(17) )
#define BIT_DDR_ZQ_PD						( BIT(16) )
#define BIT_DDR_ZQ_CAL						( BIT(15) )
#define BITS_DDR_ZQ_ZPROG(_X_)					( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_DDR_ZQ_DRVP(_X_)					( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DDR_ZQ_DRVN(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_AON_APB_WBT_BG_CTRL */
#define BIT_WBT_LDO_CAL_RST					( BIT(25) )
#define BIT_WBT_LDO_CAL_START					( BIT(24) )
#define BIT_WBT_LDO_CAL_CLK					( BIT(23) )
#define BIT_WBT_CON_BG						( BIT(22) )
#define BITS_WBT_LDO_V(_X_)					( (_X_) << 18 & (BIT(18)|BIT(19)|BIT(20)|BIT(21)) )
#define BIT_WBT_BG_RBIAS_EN					( BIT(17) )
#define BIT_WBT_BG_IEXT_IB_EN					( BIT(16) )
#define BIT_WBT_LDO_AUTO_PD_EN					( BIT(11) )
#define BIT_WBT_LDO_SLP_PD_EN					( BIT(10) )
#define BIT_WBT_LDO_FORCE_ON					( BIT(9) )
#define BIT_WBT_LDO_FORCE_PD					( BIT(8) )
#define BIT_WBT_BG_AUTO_PD_EN					( BIT(3) )
#define BIT_WBT_BG_SLP_PD_EN					( BIT(2) )
#define BIT_WBT_BG_FORCE_ON					( BIT(1) )
#define BIT_WBT_BG_FORCE_PD					( BIT(0) )

/* bits definitions for register REG_AON_APB_CLK_DMC_CFG */
#define BITS_CLK_DMC_DIV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_CLK_DMC_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AON_APB_SOFT_DFS_CTRL */
#define BITS_PUB_DFS_SW_SWITCH_PERIOD(_X_)			( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PUB_DFS_SW_RATIO(_X_)				( (_X_) << 6 & (BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )
#define BITS_PUB_DFS_SW_FRQ_SEL(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_PUB_DFS_SW_RESP					( BIT(3) )
#define BIT_PUB_DFS_SW_ACK					( BIT(2) )
#define BIT_PUB_DFS_SW_REQ					( BIT(1) )
#define BIT_PUB_DFS_SW_ENABLE					( BIT(0) )

/* bits definitions for register REG_AON_APB_CLK26M_CFG */
#define BIT_CLKBAK_EN						( BIT(4) )
#define BIT_CLK26MHZ_A2D_1_SEL					( BIT(3) )
#define BIT_CLK26MHZ_A2D_0_SEL					( BIT(2) )
#define BITS_CLK26M_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AON_APB_HARD_DFS_CTRL_LO */
#define BITS_PUB_DFS_HW_INITIAL_FREQ(_X_)			( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BIT_PUB_DFS_HW_STOP					( BIT(2) )
#define BIT_PUB_DFS_HW_START					( BIT(1) )
#define BIT_PUB_DFS_HW_ENABLE					( BIT(0) )

/* bits definitions for register REG_AON_APB_HARD_DFS_CTRL_HI */
#define BITS_PUB_DFS_HW_SWITCH_PERIOD(_X_)			( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_PUB_DFS_HW_F3_RATIO(_X_)				( (_X_) << 15 & (BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_PUB_DFS_HW_F2_RATIO(_X_)				( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_PUB_DFS_HW_F1_RATIO(_X_)				( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_PUB_DFS_HW_F0_RATIO(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_AON_APB_AON_CHIP_ID */
#define BITS_AON_CHIP_ID(_X_)					(_X_)

/* bits definitions for register REG_AON_APB_MPLL_CFG */
#define BITS_MPLL_REFIN(_X_)					( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_MPLL_LPF(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_MPLL_IBIAS(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_MPLL_N(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_DPLL_CFG */
#define BITS_DPLL_REFIN(_X_)					( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_DPLL_LPF(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_DPLL_IBIAS(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_DPLL_N(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_TDPLL_CFG */
#define BIT_TDPLL_DIV5_FORCE_PD					( BIT(31) )
#define BIT_TDPLL_DIV3_FORCE_PD					( BIT(30) )
#define BIT_TDPLL_DIV2_FORCE_PD					( BIT(29) )
#define BIT_TDPLL_DIV5_PD_AUTO					( BIT(28) )
#define BIT_TDPLL_DIV3_PD_AUTO					( BIT(27) )
#define BIT_TDPLL_DIV2_PD_AUTO					( BIT(26) )
#define BITS_TDPLL_REFIN(_X_)					( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_TDPLL_LPF(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_TDPLL_IBIAS(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_TDPLL_N(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_CPLL_CFG */
#define BITS_CPLL_REFIN(_X_)					( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_CPLL_LPF(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_CPLL_IBIAS(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_CPLL_N(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_WIFIPLL0_CFG */
#define BITS_WIFIPLL1_REFIN(_X_)				( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_WIFIPLL1_LPF(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_WIFIPLL1_IBIAS(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_WIFIPLL1_N(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_WIFIPLL1_CFG */
#define BITS_WIFIPLL2_REFIN(_X_)				( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_WIFIPLL2_LPF(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_WIFIPLL2_IBIAS(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_WIFIPLL2_N(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_WPLL_CFG0 */
#define BITS_WPLL_REFIN(_X_)					( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_WPLL_LPF(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_WPLL_IBIAS(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_WPLL_N(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_WPLL_CFG1 */
#define BITS_WPLL_KINT(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_WPLL_DIV_S						( BIT(10) )
#define BITS_WPLL_RSV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_WPLL_MOD_EN						( BIT(7) )
#define BIT_WPLL_SDM_EN						( BIT(6) )
#define BITS_WPLL_NINT(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AON_APB_AON_CGM_CFG */
#define BITS_PROBE_CKG_DIV(_X_)					( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_AUX2_CKG_DIV(_X_)					( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_AUX1_CKG_DIV(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_AUX0_CKG_DIV(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_PROBE_CKG_SEL(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_AUX2_CKG_SEL(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_AUX1_CKG_SEL(_X_)					( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_AUX0_CKG_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_CP0_ADDR_REMAP_CTRL0 */
#define BITS_CP0_ADDR_B7_REMAP(_X_)				( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_CP0_ADDR_B6_REMAP(_X_)				( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_CP0_ADDR_B5_REMAP(_X_)				( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_CP0_ADDR_B4_REMAP(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_CP0_ADDR_B3_REMAP(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CP0_ADDR_B2_REMAP(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CP0_ADDR_B1_REMAP(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CP0_ADDR_B0_REMAP(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_CP0_ADDR_REMAP_CTRL1 */
#define BIT_CP0_PUB_IRAM_B8_PROT_EN				( BIT(12) )
#define BIT_CP0_PUB_IRAM_B7_PROT_EN				( BIT(11) )
#define BIT_CP0_PUB_IRAM_B6_PROT_EN				( BIT(10) )
#define BIT_CP0_PUB_IRAM_B5_PROT_EN				( BIT(9) )
#define BIT_CP0_PUB_IRAM_B4_PROT_EN				( BIT(8) )
#define BIT_CP0_PUB_IRAM_B3_PROT_EN				( BIT(7) )
#define BIT_CP0_PUB_IRAM_B2_PROT_EN				( BIT(6) )
#define BIT_CP0_PUB_IRAM_B1_PROT_EN				( BIT(5) )
#define BIT_CP0_PUB_IRAM_B0_PROT_EN				( BIT(4) )
#define BITS_CP0_ADDR_B8_REMAP(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_CP1_ADDR_REMAP_CTRL0 */
#define BITS_CP1_ADDR_B7_REMAP(_X_)				( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_CP1_ADDR_B6_REMAP(_X_)				( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_CP1_ADDR_B5_REMAP(_X_)				( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_CP1_ADDR_B4_REMAP(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_CP1_ADDR_B3_REMAP(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CP1_ADDR_B2_REMAP(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CP1_ADDR_B1_REMAP(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CP1_ADDR_B0_REMAP(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_CP1_ADDR_REMAP_CTRL1 */
#define BIT_CP1_PUB_IRAM_B8_PROT_EN				( BIT(12) )
#define BIT_CP1_PUB_IRAM_B7_PROT_EN				( BIT(11) )
#define BIT_CP1_PUB_IRAM_B6_PROT_EN				( BIT(10) )
#define BIT_CP1_PUB_IRAM_B5_PROT_EN				( BIT(9) )
#define BIT_CP1_PUB_IRAM_B4_PROT_EN				( BIT(8) )
#define BIT_CP1_PUB_IRAM_B3_PROT_EN				( BIT(7) )
#define BIT_CP1_PUB_IRAM_B2_PROT_EN				( BIT(6) )
#define BIT_CP1_PUB_IRAM_B1_PROT_EN				( BIT(5) )
#define BIT_CP1_PUB_IRAM_B0_PROT_EN				( BIT(4) )
#define BITS_CP1_ADDR_B8_REMAP(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_CP2_ADDR_REMAP_CTRL0 */
#define BITS_CP2_ADDR_B7_REMAP(_X_)				( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_CP2_ADDR_B6_REMAP(_X_)				( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_CP2_ADDR_B5_REMAP(_X_)				( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_CP2_ADDR_B4_REMAP(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_CP2_ADDR_B3_REMAP(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CP2_ADDR_B2_REMAP(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CP2_ADDR_B1_REMAP(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CP2_ADDR_B0_REMAP(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_CP2_ADDR_REMAP_CTRL1 */
#define BIT_CP2_PUB_IRAM_B8_PROT_EN				( BIT(12) )
#define BIT_CP2_PUB_IRAM_B7_PROT_EN				( BIT(11) )
#define BIT_CP2_PUB_IRAM_B6_PROT_EN				( BIT(10) )
#define BIT_CP2_PUB_IRAM_B5_PROT_EN				( BIT(9) )
#define BIT_CP2_PUB_IRAM_B4_PROT_EN				( BIT(8) )
#define BIT_CP2_PUB_IRAM_B3_PROT_EN				( BIT(7) )
#define BIT_CP2_PUB_IRAM_B2_PROT_EN				( BIT(6) )
#define BIT_CP2_PUB_IRAM_B1_PROT_EN				( BIT(5) )
#define BIT_CP2_PUB_IRAM_B0_PROT_EN				( BIT(4) )
#define BITS_CP2_ADDR_B8_REMAP(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_IO_DLY_CTRL */
#define BITS_CLK_CCIR_DLY_SEL(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CLK_CP1DSP_DLY_SEL(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CLK_CP0DSP_DLY_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_AP_WPROT_EN */
#define BITS_AP_AWADDR_WPROT_EN(_X_)				(_X_)

/* bits definitions for register REG_AON_APB_CP0_WPROT_EN */
#define BITS_CP0_AWADDR_WPROT_EN(_X_)				(_X_)

/* bits definitions for register REG_AON_APB_CP1_WPROT_EN */
#define BITS_CP1_AWADDR_WPROT_EN(_X_)				(_X_)

/* bits definitions for register REG_AON_APB_CP2_WPROT_EN */
#define BITS_CP2_AWADDR_WPROT_EN(_X_)				(_X_)

/* bits definitions for register REG_AON_APB_PMU_RST_MONITOR */
#define BITS_PMU_RST_MONITOR(_X_)				(_X_)

/* bits definitions for register REG_AON_APB_THM_RST_MONITOR */
#define BITS_THM_RST_MONITOR(_X_)				(_X_)

/* bits definitions for register REG_AON_APB_AP_RST_MONITOR */
#define BITS_AP_RST_MONITOR(_X_)				(_X_)

/* bits definitions for register REG_AON_APB_CA7_RST_MONITOR */
#define BITS_CA7_RST_MONITOR(_X_)				(_X_)

/* bits definitions for register REG_AON_APB_BOND_OPT0 */
#define BITS_BOND_OPTION0(_X_)					(_X_)

/* bits definitions for register REG_AON_APB_BOND_OPT1 */
#define BITS_BOND_OPTION1(_X_)					(_X_)

/* bits definitions for register REG_AON_APB_RES_REG0 */
#define BITS_RES_REG0(_X_)					(_X_)

/* bits definitions for register REG_AON_APB_RES_REG1 */
#define BITS_RES_REG1(_X_)					(_X_)

/* bits definitions for register REG_AON_APB_MPLL_CFG1 */
#define BITS_MPLL_KINT(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_MPLL_DIV_S						( BIT(10) )
#define BITS_MPLL_RSV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_MPLL_MOD_EN						( BIT(7) )
#define BIT_MPLL_SDM_EN						( BIT(6) )
#define BITS_MPLL_NINT(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AON_APB_DPLL_CFG1 */
#define BITS_DPLL_KINT(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_DPLL_DIV_S						( BIT(10) )
#define BITS_DPLL_RSV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_DPLL_MOD_EN						( BIT(7) )
#define BIT_DPLL_SDM_EN						( BIT(6) )
#define BITS_DPLL_NINT(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AON_APB_TDPLL_CFG1 */
#define BITS_TDPLL_KINT(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_TDPLL_DIV_S						( BIT(10) )
#define BITS_TDPLL_RSV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_TDPLL_MOD_EN					( BIT(7) )
#define BIT_TDPLL_SDM_EN					( BIT(6) )
#define BITS_TDPLL_NINT(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AON_APB_CPLL_CFG1 */
#define BITS_CPLL_KINT(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_CPLL_DIV_S						( BIT(10) )
#define BITS_CPLL_RSV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_CPLL_MOD_EN						( BIT(7) )
#define BIT_CPLL_SDM_EN						( BIT(6) )
#define BITS_CPLL_NINT(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AON_APB_WIFIPLL1_CFG1 */
#define BITS_WIFIPLL1_KINT(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_WIFIPLL1_DIV_S					( BIT(10) )
#define BITS_WIFIPLL1_RSV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_WIFIPLL1_MOD_EN					( BIT(7) )
#define BIT_WIFIPLL1_SDM_EN					( BIT(6) )
#define BITS_WIFIPLL1_NINT(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AON_APB_WIFIPLL2_CFG1 */
#define BITS_WIFIPLL2_KINT(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_WIFIPLL2_DIV_S					( BIT(10) )
#define BITS_WIFIPLL2_RSV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_WIFIPLL2_MOD_EN					( BIT(7) )
#define BIT_WIFIPLL2_SDM_EN					( BIT(6) )
#define BITS_WIFIPLL2_NINT(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AON_APB_AON_QOS_CFG */
#define BITS_QOS_R_GPU(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_QOS_W_GPU(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_QOS_R_GSP(_X_)					( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_QOS_W_GSP(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_BB_LDO_CAL_START */
#define BIT_BB_LDO_CAL_START					( BIT(0) )

/* bits definitions for register REG_AON_APB_WBT_BG_LDO */
#define BITS_WBT_BG_RESERVED(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_WBT_RESERVED(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AON_APB_ANA_BB_MISC */
#define BITS_ANA_BB_RESERVED(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AON_APB_DJTAG_MUX_SEL */
#define BIT_DJTAG_AON_SEL					( BIT(6) )
#define BIT_DJTAG_PUB_SEL					( BIT(5) )
#define BIT_DJTAG_CP2_SEL					( BIT(4) )
#define BIT_DJTAG_CP0_SEL					( BIT(3) )
#define BIT_DJTAG_GPU_SEL					( BIT(2) )
#define BIT_DJTAG_MM_SEL					( BIT(1) )
#define BIT_DJTAG_AP_SEL					( BIT(0) )

/* bits definitions for register REG_AON_APB_BBPLL_RESERVED_0 */
#define BITS_WPLL_RESERVED(_X_)					( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_TDPLL_RESERVED(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_MPLL_RESERVED(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CPLL_RESERVED(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AON_APB_BBPLL_RESERVED_1 */
#define BITS_LVDSRF_LVPLL_RESERVED(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_RPLL_RESERVED(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AON_APB_RPLL_CFG0 */
#define BIT_LVDS_LVPLL_REF_SEL_CTRL				( BIT(10) )
#define BIT_LVDS_LVPLL_REF_SEL					( BIT(9) )
#define BITS_RPLL_PD_CTRL(_X_)					( (_X_) << 7 & (BIT(7)|BIT(8)) )
#define BIT_RPLL_RST_CTRL					( BIT(6) )
#define BIT_BBPLL_REFCLK_SEL_CTRL				( BIT(5) )
#define BIT_RPLL_26MTESTOUT_EN					( BIT(4) )
#define BIT_CLK26MHZ_MUXTUNE_EN					( BIT(3) )
#define BIT_BBPLL_REFCLK_SEL					( BIT(2) )
#define BIT_RPLL_RST						( BIT(1) )
#define BIT_RPLL_PD						( BIT(0) )

/* bits definitions for register REG_AON_APB_AON_DEBUG_CFG */
#define BITS_AON_DEBUG_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

#endif
