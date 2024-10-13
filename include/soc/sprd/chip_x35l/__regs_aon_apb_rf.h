/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
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

#define	REGS_AON_APB_

/* registers definitions for AON_APB_ */
#define REG_AON_APB_APB_EB0                            SCI_ADDR(REGS_AON_APB_BASE, 0x0000)
#define REG_AON_APB_APB_EB1                            SCI_ADDR(REGS_AON_APB_BASE, 0x0004)
#define REG_AON_APB_APB_RST0                           SCI_ADDR(REGS_AON_APB_BASE, 0x0008)
#define REG_AON_APB_APB_RST1                           SCI_ADDR(REGS_AON_APB_BASE, 0x000C)
#define REG_AON_APB_APB_RTC_EB                         SCI_ADDR(REGS_AON_APB_BASE, 0x0010)
#define REG_AON_APB_REC_26MHZ_BUF_CFG                  SCI_ADDR(REGS_AON_APB_BASE, 0x0014)
#define REG_AON_APB_SINDRV_CTRL                        SCI_ADDR(REGS_AON_APB_BASE, 0x0018)
#define REG_AON_APB_ADA_SEL_CTRL                       SCI_ADDR(REGS_AON_APB_BASE, 0x001C)
#define REG_AON_APB_VBC_CTRL                           SCI_ADDR(REGS_AON_APB_BASE, 0x0020)
#define REG_AON_APB_PWR_CTRL                           SCI_ADDR(REGS_AON_APB_BASE, 0x0024)
#define REG_AON_APB_TS_CFG                             SCI_ADDR(REGS_AON_APB_BASE, 0x0028)
#define REG_AON_APB_BOOT_MODE                          SCI_ADDR(REGS_AON_APB_BASE, 0x002C)
#define REG_AON_APB_BB_BG_CTRL                         SCI_ADDR(REGS_AON_APB_BASE, 0x0030)
#define REG_AON_APB_CP_ARM_JTAG_CTRL                   SCI_ADDR(REGS_AON_APB_BASE, 0x0034)
#define REG_AON_APB_PLL_SOFT_CNT_DONE                  SCI_ADDR(REGS_AON_APB_BASE, 0x0038)
#define REG_AON_APB_DCXO_LC_REG0                       SCI_ADDR(REGS_AON_APB_BASE, 0x003C)
#define REG_AON_APB_DCXO_LC_REG1                       SCI_ADDR(REGS_AON_APB_BASE, 0x0040)
#define REG_AON_APB_MPLL_CFG1                          SCI_ADDR(REGS_AON_APB_BASE, 0x0044)
#define REG_AON_APB_MPLL_CFG2                          SCI_ADDR(REGS_AON_APB_BASE, 0x0048)
#define REG_AON_APB_DPLL_CFG1                          SCI_ADDR(REGS_AON_APB_BASE, 0x004C)
#define REG_AON_APB_DPLL_CFG2                          SCI_ADDR(REGS_AON_APB_BASE, 0x0050)
#define REG_AON_APB_TWPLL_CFG1                         SCI_ADDR(REGS_AON_APB_BASE, 0x0054)
#define REG_AON_APB_TWPLL_CFG2                         SCI_ADDR(REGS_AON_APB_BASE, 0x0058)
#define REG_AON_APB_LTEPLL_CFG1                        SCI_ADDR(REGS_AON_APB_BASE, 0x005C)
#define REG_AON_APB_LTEPLL_CFG2                        SCI_ADDR(REGS_AON_APB_BASE, 0x0060)
#define REG_AON_APB_LVDSDISPLL_CFG1                    SCI_ADDR(REGS_AON_APB_BASE, 0x0064)
#define REG_AON_APB_LVDSDISPLL_CFG2                    SCI_ADDR(REGS_AON_APB_BASE, 0x0068)
#define REG_AON_APB_AON_REG_PROT                       SCI_ADDR(REGS_AON_APB_BASE, 0x006C)/*Big endian protect register*/
#define REG_AON_APB_LDSP_BOOT_EN                       SCI_ADDR(REGS_AON_APB_BASE, 0x0070)/*DSP boot enable*/
#define REG_AON_APB_LDSP_BOOT_VEC                      SCI_ADDR(REGS_AON_APB_BASE, 0x0074)/*DSP boot vector*/
#define REG_AON_APB_LDSP_RST                           SCI_ADDR(REGS_AON_APB_BASE, 0x0078)/*DSP reset*/
#define REG_AON_APB_LDSP_MTX_CTRL1                     SCI_ADDR(REGS_AON_APB_BASE, 0x007C)
#define REG_AON_APB_LDSP_MTX_CTRL2                     SCI_ADDR(REGS_AON_APB_BASE, 0x0080)
#define REG_AON_APB_LDSP_MTX_CTRL3                     SCI_ADDR(REGS_AON_APB_BASE, 0x0084)
#define REG_AON_APB_AON_CGM_CFG                        SCI_ADDR(REGS_AON_APB_BASE, 0x0088)
#define REG_AON_APB_LACC_MTX_CTRL                      SCI_ADDR(REGS_AON_APB_BASE, 0x008C)
#define REG_AON_APB_CORTEX_MTX_CTRL1                   SCI_ADDR(REGS_AON_APB_BASE, 0x0090)
#define REG_AON_APB_CORTEX_MTX_CTRL2                   SCI_ADDR(REGS_AON_APB_BASE, 0x0094)
#define REG_AON_APB_CORTEX_MTX_CTRL3                   SCI_ADDR(REGS_AON_APB_BASE, 0x0098)/*DSP reset*/
#define REG_AON_APB_CA5_TCLK_DLY_LEN                   SCI_ADDR(REGS_AON_APB_BASE, 0x009C)/*APB clock control*/
#define REG_AON_APB_CCIR_RCVR_CFG                      SCI_ADDR(REGS_AON_APB_BASE, 0x0100)/*APB clock control*/
#define REG_AON_APB_AON_CHIP_ID_H                      SCI_ADDR(REGS_AON_APB_BASE, 0x00F8)
#define REG_AON_APB_AON_CHIP_ID                        SCI_ADDR(REGS_AON_APB_BASE, 0x00FC)
#define REG_AON_APB_AON_CHIP_ID_L                      REG_AON_APB_AON_CHIP_ID
#define REG_AON_APB_PLL_BG_CFG                         SCI_ADDR(REGS_AON_APB_BASE, 0x0108)
#define REG_AON_APB_LVDSDIS_SEL                        SCI_ADDR(REGS_AON_APB_BASE, 0x010C)
#define REG_AON_APB_DJTAG_MUX_SEL                      SCI_ADDR(REGS_AON_APB_BASE, 0x0110)
#define REG_AON_APB_ARM7_SYS_SOFT_RST                  SCI_ADDR(REGS_AON_APB_BASE, 0x0114)
#define REG_AON_APB_CP1_CP0_ADDR_MSB                   SCI_ADDR(REGS_AON_APB_BASE, 0x0118)
#define REG_AON_APB_AON_DMA_INT_EN                     SCI_ADDR(REGS_AON_APB_BASE, 0x011C)
#define REG_AON_APB_EMC_AUTO_GATE_EN                   SCI_ADDR(REGS_AON_APB_BASE, 0x0120)
#define REG_AON_APB_ARM7_CFG_BUS                       SCI_ADDR(REGS_AON_APB_BASE, 0x0124)
#define REG_AON_APB_RTC4M_0_CFG                        SCI_ADDR(REGS_AON_APB_BASE, 0x0128)
#define REG_AON_APB_RTC4M_1_CFG                        SCI_ADDR(REGS_AON_APB_BASE, 0x012C)
#define REG_AON_APB_APB_RST2                           SCI_ADDR(REGS_AON_APB_BASE, 0x0130)
#define REG_AON_APB_AP_WPROT_EN1                       SCI_ADDR(REGS_AON_APB_BASE, 0x3004)
#define REG_AON_APB_CP0_WPROT_EN1                      SCI_ADDR(REGS_AON_APB_BASE, 0x3008)
#define REG_AON_APB_CP1_WPROT_EN1                      SCI_ADDR(REGS_AON_APB_BASE, 0x300C)
#define REG_AON_APB_IO_DLY_CTRL                        SCI_ADDR(REGS_AON_APB_BASE, 0x3014)
#define REG_AON_APB_AP_WPROT_EN0                       SCI_ADDR(REGS_AON_APB_BASE, 0x3018)
#define REG_AON_APB_CP0_WPROT_EN0                      SCI_ADDR(REGS_AON_APB_BASE, 0x3020)
#define REG_AON_APB_CP1_WPROT_EN0                      SCI_ADDR(REGS_AON_APB_BASE, 0x3024)
#define REG_AON_APB_PMU_RST_MONITOR                    SCI_ADDR(REGS_AON_APB_BASE, 0x302C)
#define REG_AON_APB_THM_RST_MONITOR                    SCI_ADDR(REGS_AON_APB_BASE, 0x3030)
#define REG_AON_APB_AP_RST_MONITOR                     SCI_ADDR(REGS_AON_APB_BASE, 0x3034)
#define REG_AON_APB_CA7_RST_MONITOR                    SCI_ADDR(REGS_AON_APB_BASE, 0x3038)
#define REG_AON_APB_BOND_OPT0                          SCI_ADDR(REGS_AON_APB_BASE, 0x303C)
#define REG_AON_APB_BOND_OPT1                          SCI_ADDR(REGS_AON_APB_BASE, 0x3040)
#define REG_AON_APB_RES_REG0                           SCI_ADDR(REGS_AON_APB_BASE, 0x3044)
#define REG_AON_APB_RES_REG1                           SCI_ADDR(REGS_AON_APB_BASE, 0x3048)
#define REG_AON_APB_AON_QOS_CFG                        SCI_ADDR(REGS_AON_APB_BASE, 0x304C)
#define REG_AON_APB_BB_LDO_CAL_START                   SCI_ADDR(REGS_AON_APB_BASE, 0x3050)
#define REG_AON_APB_AON_MTX_PROT_CFG                   SCI_ADDR(REGS_AON_APB_BASE, 0x3058)
#define REG_AON_APB_LVDS_CFG                           SCI_ADDR(REGS_AON_APB_BASE, 0x3060)
#define REG_AON_APB_PLL_LOCK_OUT_SEL                   SCI_ADDR(REGS_AON_APB_BASE, 0x3064)
#define REG_AON_APB_RTC4M_RC_VAL                       SCI_ADDR(REGS_AON_APB_BASE, 0x3068)
#define REG_AON_APB_AON_APB_RSV                        SCI_ADDR(REGS_AON_APB_BASE, 0x30F0)

/* bits definitions for register REG_AON_APB_APB_EB0 */
#define BIT_I2C_EB                                        ( BIT(31) )
#define BIT_CA7_DAP_EB                                    ( BIT(30) )
#define BIT_CA7_TS1_EB                                    ( BIT(29) )
#define BIT_CA7_TS0_EB                                    ( BIT(28) )
#define BIT_GPU_EB                                        ( BIT(27) )
#define BIT_AON_CKG_EB                                     ( BIT(26) )
#define BIT_MM_EB                                         ( BIT(25) )
#define BIT_AP_WDG_EB                                     ( BIT(24) )
#define BIT_MSPI_EB                                       ( BIT(23) )
#define BIT_SPLK_EB                                       ( BIT(22) )
#define BIT_IPI_EB                                        ( BIT(21) )
#define BIT_PIN_EB                                        ( BIT(20) )
#define BIT_VBC_EB                                        ( BIT(19) )
#define BIT_AUD_EB                                        ( BIT(18) )
#define BIT_AUDIF_EB                                      ( BIT(17) )
#define BIT_ADI_EB                                        ( BIT(16) )
#define BIT_INTC_EB                                       ( BIT(15) )
#define BIT_EIC_EB                                        ( BIT(14) )
#define BIT_EFUSE_EB                                      ( BIT(13) )
#define BIT_AP_TMR0_EB                                    ( BIT(12) )
#define BIT_AON_TMR_EB                                    ( BIT(11) )
#define BIT_AP_SYST_EB                                    ( BIT(10) )
#define BIT_AON_SYST_EB                                   ( BIT(9) )
#define BIT_KPD_EB                                        ( BIT(8) )
#define BIT_PWM3_EB                                       ( BIT(7) )
#define BIT_PWM2_EB                                       ( BIT(6) )
#define BIT_PWM1_EB                                       ( BIT(5) )
#define BIT_PWM0_EB                                       ( BIT(4) )
#define BIT_GPIO_EB                                       ( BIT(3) )
#define BIT_TPC_EB                                        ( BIT(2) )
#define BIT_FM_EB                                         ( BIT(1) )
#define BIT_ADC_EB                                        ( BIT(0) )

/* bits definitions for register REG_AON_APB_APB_EB1 */
#if defined(CONFIG_MACH_SP9830I)
#define BIT_CODEC_EB                                      (BIT(28))
#endif
#define BIT_ORP_JTAG_EB                                   ( BIT(27) )
#define BIT_CA5_TS0_EB                                    ( BIT(26) )
#define BIT_DEF_EB                                        ( BIT(25) )
#define BIT_LVDS_PLL_DIV_EN                               ( BIT(24) )
#define BIT_ARM7_JTAG_EB                                  ( BIT(23) )
#define BIT_AON_DMA_EB                                    ( BIT(22) )
#define BIT_MBOX_EB                                       ( BIT(21) )
#define BIT_DJTAG_EB                                      ( BIT(20) )
#define BIT_RTC4M1_CAL_EB                                 ( BIT(19) )
#define BIT_RTC4M0_CAL_EB                                 ( BIT(18) )
#define BIT_MDAR_EB                                       ( BIT(17) )
#define BIT_LVDS_TCXO_EB                                  ( BIT(16) )
#define BIT_LVDS_TRX_EB                                   ( BIT(15) )
#define BIT_CA5_DAP_EB                                    ( BIT(14) )
#define BIT_GSP_EMC_EB                                    ( BIT(13) )
#define BIT_ZIP_EMC_EB                                    ( BIT(12) )
#define BIT_DISP_EMC_EB                                   ( BIT(11) )
#define BIT_AP_TMR2_EB                                    ( BIT(10) )
#define BIT_AP_TMR1_EB                                    ( BIT(9) )
#define BIT_CA7_WDG_EB                                    ( BIT(8) )
#define BIT_AVS_EB                                        ( BIT(6) )
#define BIT_PROBE_EB                                      ( BIT(5) )
#define BIT_AUX2_EB                                       ( BIT(4) )
#define BIT_AUX1_EB                                       ( BIT(3) )
#define BIT_AUX0_EB                                       ( BIT(2) )
#define BIT_THM_EB                                        ( BIT(1) )
#define BIT_PMU_EB                                        ( BIT(0) )

/* bits definitions for register REG_AON_APB_APB_RST0 */
#define BIT_CA5_TS0_SOFT_RST                              ( BIT(31) )
#define BIT_I2C_SOFT_RST                                  ( BIT(30) )
#define BIT_CA7_TS1_SOFT_RST                              ( BIT(29) )
#define BIT_CA7_TS0_SOFT_RST                              ( BIT(28) )
#define BIT_DAP_MTX_SOFT_RST                              ( BIT(27) )
#define BIT_MSPI1_SOFT_RST                                ( BIT(26) )
#define BIT_MSPI0_SOFT_RST                                ( BIT(25) )
#define BIT_SPLK_SOFT_RST                                 ( BIT(24) )
#define BIT_IPI_SOFT_RST                                  ( BIT(23) )
#define BIT_AON_CKG_SOFT_RST                               ( BIT(22) )
#define BIT_PIN_SOFT_RST                                  ( BIT(21) )
#define BIT_VBC_SOFT_RST                                  ( BIT(20) )
#define BIT_AUD_SOFT_RST                                  ( BIT(19) )
#define BIT_AUDIF_SOFT_RST                                ( BIT(18) )
#define BIT_ADI_SOFT_RST                                  ( BIT(17) )
#define BIT_INTC_SOFT_RST                                 ( BIT(16) )
#define BIT_EIC_SOFT_RST                                  ( BIT(15) )
#define BIT_EFUSE_SOFT_RST                                ( BIT(14) )
#define BIT_AP_WDG_SOFT_RST                               ( BIT(13) )
#define BIT_AP_TMR0_SOFT_RST                              ( BIT(12) )
#define BIT_AON_TMR_SOFT_RST                              ( BIT(11) )
#define BIT_AP_SYST_SOFT_RST                              ( BIT(10) )
#define BIT_AON_SYST_SOFT_RST                             ( BIT(9) )
#define BIT_KPD_SOFT_RST                                  ( BIT(8) )
#define BIT_PWM3_SOFT_RST                                 ( BIT(7) )
#define BIT_PWM2_SOFT_RST                                 ( BIT(6) )
#define BIT_PWM1_SOFT_RST                                 ( BIT(5) )
#define BIT_PWM0_SOFT_RST                                 ( BIT(4) )
#define BIT_GPIO_SOFT_RST                                 ( BIT(3) )
#define BIT_TPC_SOFT_RST                                  ( BIT(2) )
#define BIT_FM_SOFT_RST                                   ( BIT(1) )
#define BIT_ADC_SOFT_RST                                  ( BIT(0) )

/* bits definitions for register REG_AON_APB_APB_RST1 */
#define BIT_RTC4M_ANA_SOFT_RST                            ( BIT(31) )
#define BIT_DEF_SLV_INT_SOFT_CLR                          ( BIT(30) )
#define BIT_DEF_SOFT_RST                                  ( BIT(29) )
#define BIT_ADC3_SOFT_RST                                 ( BIT(28) )
#define BIT_ADC2_SOFT_RST                                 ( BIT(27) )
#define BIT_ADC1_SOFT_RST                                 ( BIT(26) )
#define BIT_MBOX_SOFT_RST                                 ( BIT(25) )
#define BIT_RTC4M1_CAL_SOFT_RST                           ( BIT(23) )
#define BIT_RTC4M0_CAL_SOFT_RST                           ( BIT(22) )
#define BIT_LDSP_SYS_SOFT_RST                             ( BIT(21) )
#define BIT_LCP_SYS_SOFT_RST                              ( BIT(20) )
#define BIT_DAC3_SOFT_RST                                 ( BIT(19) )
#define BIT_DAC2_SOFT_RST                                 ( BIT(18) )
#define BIT_DAC1_SOFT_RST                                 ( BIT(17) )
#define BIT_ADC3_CAL_SOFT_RST                             ( BIT(16) )
#define BIT_ADC2_CAL_SOFT_RST                             ( BIT(15) )
#define BIT_ADC1_CAL_SOFT_RST                             ( BIT(14) )
#define BIT_MDAR_SOFT_RST                                 ( BIT(13) )
#define BIT_LVDSDIS_SOFT_RST                              ( BIT(12) )
#define BIT_BB_CAL_SOFT_RST                               ( BIT(11) )
#define BIT_DCXO_LC_SOFT_RST                              ( BIT(10) )
#define BIT_AP_TMR2_SOFT_RST                              ( BIT(9) )
#define BIT_AP_TMR1_SOFT_RST                              ( BIT(8) )
#define BIT_CA7_WDG_SOFT_RST                              ( BIT(7) )
#define BIT_AON_DMA_SOFT_RST                              ( BIT(6) )
#define BIT_AVS_SOFT_RST                                  ( BIT(5) )
#define BIT_DMC_PHY_SOFT_RST                              ( BIT(4) )
#define BIT_GPU_THMA_SOFT_RST                             ( BIT(3) )
#define BIT_ARM_THMA_SOFT_RST                             ( BIT(2) )
#define BIT_THM_SOFT_RST                                  ( BIT(1) )
#define BIT_PMU_SOFT_RST                                  ( BIT(0) )

/* bits definitions for register REG_AON_APB_APB_RTC_EB */
#define BIT_CP0_LTE_EB                                    ( BIT(19) )
#define BIT_BB_CAL_RTC_EB                                 ( BIT(18) )
#define BIT_DCXO_LC_RTC_EB                                ( BIT(17) )
#define BIT_AP_TMR2_RTC_EB                                ( BIT(16) )
#define BIT_AP_TMR1_RTC_EB                                ( BIT(15) )
#define BIT_GPU_THMA_RTC_AUTO_EN                          ( BIT(14) )
#define BIT_ARM_THMA_RTC_AUTO_EN                          ( BIT(13) )
#define BIT_GPU_THMA_RTC_EB                               ( BIT(12) )
#define BIT_ARM_THMA_RTC_EB                               ( BIT(11) )
#define BIT_THM_RTC_EB                                    ( BIT(10) )
#define BIT_CA7_WDG_RTC_EB                                ( BIT(9) )
#define BIT_AP_WDG_RTC_EB                                 ( BIT(8) )
#define BIT_EIC_RTCDV5_EB                                 ( BIT(7) )
#define BIT_EIC_RTC_EB                                    ( BIT(6) )
#define BIT_AP_TMR0_RTC_EB                                ( BIT(5) )
#define BIT_AON_TMR_RTC_EB                                ( BIT(4) )
#define BIT_AP_SYST_RTC_EB                                ( BIT(3) )
#define BIT_AON_SYST_RTC_EB                               ( BIT(2) )
#define BIT_KPD_RTC_EB                                    ( BIT(1) )
#define BIT_ARCH_RTC_EB                                   ( BIT(0) )

/* bits definitions for register REG_AON_APB_REC_26MHZ_BUF_CFG */
#define BITS_PLL_PROBE_SEL(_X_)                           ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_REC_26MHZ_1_CUR_SEL                           ( BIT(4) )
#define BIT_REC_26MHZ_0_CUR_SEL                           ( BIT(0) )

/* bits definitions for register REG_AON_APB_SINDRV_CTRL */
#define BITS_SINDRV_LVL(_X_)                              ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BIT_SINDRV_CLIP_MODE                              ( BIT(2) )
#define BIT_SINDRV_ENA_SQUARE                             ( BIT(1) )
#define BIT_SINDRV_ENA                                    ( BIT(0) )

/* bits definitions for register REG_AON_APB_ADA_SEL_CTRL */
#define BIT_TW_MODE_SEL                                   ( BIT(3) )
#define BIT_WGADC_DIV_EN                                  ( BIT(2) )
#define BIT_AFCDAC_SYS_SEL                                ( BIT(1) )
#define BIT_APCDAC_SYS_SEL                                ( BIT(0) )

/* bits definitions for register REG_AON_APB_VBC_CTRL */
#define BIT_AUDIF_CKG_AUTO_EN                             ( BIT(20) )
#define BITS_AUD_INT_SYS_SEL(_X_)                         ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_VBC_AFIFO_INT_SYS_SEL(_X_)                   ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_VBC_AD23_INT_SYS_SEL(_X_)                    ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_VBC_AD01_INT_SYS_SEL(_X_)                    ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_VBC_DA01_INT_SYS_SEL(_X_)                    ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_VBC_AD23_DMA_SYS_SEL(_X_)                    ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_VBC_AD01_DMA_SYS_SEL(_X_)                    ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_VBC_DA01_DMA_SYS_SEL(_X_)                    ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_VBC_INT_CP0_ARM_SEL                           ( BIT(3) )
#define BIT_VBC_INT_CP1_ARM_SEL                           ( BIT(2) )
#define BIT_VBC_DMA_CP0_ARM_SEL                           ( BIT(1) )
#define BIT_VBC_DMA_CP1_ARM_SEL                           ( BIT(0) )

/* bits definitions for register REG_AON_APB_PWR_CTRL */
#define BIT_HSIC_PLL_EN                                   ( BIT(19) )
#define BIT_HSIC_PHY_PD                                   ( BIT(18) )
#define BIT_HSIC_PS_PD_S                                  ( BIT(17) )
#define BIT_HSIC_PS_PD_L                                  ( BIT(16) )
#define BIT_MIPI_DSI_PS_PD_S                              ( BIT(15) )
#define BIT_MIPI_DSI_PS_PD_L                              ( BIT(14) )
#define BIT_MIPI_CSI_4LANE_PS_PD_S                        ( BIT(13) )
#define BIT_MIPI_CSI_4LANE_PS_PD_L                        ( BIT(12) )
#define BIT_MIPI_CSI_2LANE_PS_PD_S                        ( BIT(11) )
#define BIT_MIPI_CSI_2LANE_PS_PD_L                        ( BIT(10) )
#define BIT_CA7_TS1_STOP                                  ( BIT(9) )
#define BIT_CA7_TS0_STOP                                  ( BIT(8) )
#define BIT_EFUSE_BIST_PWR_ON                             ( BIT(3) )
#define BIT_FORCE_DSI_PHY_SHUTDOWNZ                       ( BIT(2) )
#define BIT_FORCE_CSI_PHY_SHUTDOWNZ                       ( BIT(1) )
#define BIT_USB_PHY_PD                                    ( BIT(0) )

#define BIT_CSI1_PHY_PD                                   ( BIT(11) )
#define BIT_CSI0_PHY_PD                                   ( BIT(10) )

/* bits definitions for register REG_AON_APB_TS_CFG */
#define BIT_CSYSACK_TS_LP_2                               ( BIT(13) )
#define BIT_CSYSREQ_TS_LP_2                               ( BIT(12) )
#define BIT_CSYSACK_TS_LP_1                               ( BIT(11) )
#define BIT_CSYSREQ_TS_LP_1                               ( BIT(10) )
#define BIT_CSYSACK_TS_LP_0                               ( BIT(9) )
#define BIT_CSYSREQ_TS_LP_0                               ( BIT(8) )
#define BIT_EVENTACK_RESTARTREQ_TS01                      ( BIT(4) )
#define BIT_EVENT_RESTARTREQ_TS01                         ( BIT(1) )
#define BIT_EVENT_HALTREQ_TS01                            ( BIT(0) )

/* bits definitions for register REG_AON_APB_BOOT_MODE */
#define BIT_ARM_JTAG_EN                                   ( BIT(13) )
#define BIT_WPLL_OVR_FREQ_SEL                             ( BIT(12) )
#define BIT_PTEST_FUNC_ATSPEED_SEL                        ( BIT(8) )
#define BIT_PTEST_FUNC_MODE                               ( BIT(7) )
#define BIT_USB_DLOAD_EN                                  ( BIT(4) )
#define BIT_ARM_BOOT_MD3                                  ( BIT(3) )
#define BIT_ARM_BOOT_MD2                                  ( BIT(2) )
#define BIT_ARM_BOOT_MD1                                  ( BIT(1) )
#define BIT_ARM_BOOT_MD0                                  ( BIT(0) )

/* bits definitions for register REG_AON_APB_BB_BG_CTRL */
#define BIT_BB_CON_BG                                     ( BIT(22) )
#define BITS_BB_BG_RSV(_X_)                               ( (_X_) << 20 & (BIT(20)|BIT(21)) )
#define BITS_BB_LDO_V(_X_)                                ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BIT_BB_BG_RBIAS_EN                                ( BIT(15) )
#define BIT_BB_BG_IEXT_IB_EN                              ( BIT(14) )
#define BITS_BB_LDO_REFCTRL(_X_)                          ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BIT_BB_LDO_AUTO_PD_EN                             ( BIT(11) )
#define BIT_BB_LDO_SLP_PD_EN                              ( BIT(10) )
#define BIT_BB_LDO_FORCE_ON                               ( BIT(9) )
#define BIT_BB_LDO_FORCE_PD                               ( BIT(8) )
#define BIT_BB_BG_AUTO_PD_EN                              ( BIT(3) )
#define BIT_BB_BG_SLP_PD_EN                               ( BIT(2) )
#define BIT_BB_BG_FORCE_ON                                ( BIT(1) )
#define BIT_BB_BG_FORCE_PD                                ( BIT(0) )

/* bits definitions for register REG_AON_APB_CP_ARM_JTAG_CTRL */
#define BITS_CP_ARM_JTAG_PIN_SEL(_X_)                     ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AON_APB_PLL_SOFT_CNT_DONE */
#define BIT_RC1_SOFT_CNT_DONE                             ( BIT(13) )
#define BIT_RC0_SOFT_CNT_DONE                             ( BIT(12) )
#define BIT_XTLBUF1_SOFT_CNT_DONE                         ( BIT(9) )
#define BIT_XTLBUF0_SOFT_CNT_DONE                         ( BIT(8) )
#define BIT_LVDSPLL_SOFT_CNT_DONE                         ( BIT(4) )
#define BIT_LPLL_SOFT_CNT_DONE                            ( BIT(3) )
#define BIT_TWPLL_SOFT_CNT_DONE                           ( BIT(2) )
#define BIT_DPLL_SOFT_CNT_DONE                            ( BIT(1) )
#define BIT_MPLL_SOFT_CNT_DONE                            ( BIT(0) )

/* bits definitions for register REG_AON_APB_DCXO_LC_REG0 */
#define BIT_DCXO_LC_FLAG                                  ( BIT(8) )
#define BIT_DCXO_LC_FLAG_CLR                              ( BIT(1) )
#define BIT_DCXO_LC_CNT_CLR                               ( BIT(0) )

/* bits definitions for register REG_AON_APB_DCXO_LC_REG1 */
#define BITS_DCXO_LC_CNT(_X_)                             (_X_)

/* bits definitions for register REG_AON_APB_MPLL_CFG1 */
#define BITS_MPLL_RES(_X_)                                ( (_X_) << 28 & (BIT(28)|BIT(29)) )
#define BIT_MPLL_LOCK_DONE                                ( BIT(27) )
#define BIT_MPLL_DIV_S                                    ( BIT(26) )
#define BIT_MPLL_MOD_EN                                   ( BIT(25) )
#define BIT_MPLL_SDM_EN                                   ( BIT(24) )
#define BITS_MPLL_LPF(_X_)                                ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_MPLL_REFIN(_X_)                              ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_MPLL_IBIAS(_X_)                              ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_MPLL_N(_X_)                                  ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_MPLL_CFG2 */
#define BITS_MPLL_NINT(_X_)                               ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BITS_MPLL_KINT(_X_)                               ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )

/* bits definitions for register REG_AON_APB_DPLL_CFG1 */
#define BITS_DPLL_RES(_X_)                                ( (_X_) << 28 & (BIT(28)|BIT(29)) )
#define BIT_DPLL_LOCK_DONE                                ( BIT(27) )
#define BIT_DPLL_DIV_S                                    ( BIT(26) )
#define BIT_DPLL_MOD_EN                                   ( BIT(25) )
#define BIT_DPLL_SDM_EN                                   ( BIT(24) )
#define BITS_DPLL_LPF(_X_)                                ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_DPLL_REFIN(_X_)                              ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_DPLL_IBIAS(_X_)                              ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_DPLL_N(_X_)                                  ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_DPLL_CFG2 */
#define BITS_DPLL_NINT(_X_)                               ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BITS_DPLL_KINT(_X_)                               ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )

/* bits definitions for register REG_AON_APB_TWPLL_CFG1 */
#define BITS_TWPLL_RES(_X_)                               ( (_X_) << 28 & (BIT(28)|BIT(29)) )
#define BIT_TWPLL_LOCK_DONE                               ( BIT(27) )
#define BIT_TWPLL_DIV_S                                   ( BIT(26) )
#define BIT_TWPLL_MOD_EN                                  ( BIT(25) )
#define BIT_TWPLL_SDM_EN                                  ( BIT(24) )
#define BITS_TWPLL_LPF(_X_)                               ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_TWPLL_REFIN(_X_)                             ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_TWPLL_IBIAS(_X_)                             ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_TWPLL_N(_X_)                                 ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_TWPLL_CFG2 */
#define BITS_TWPLL_NINT(_X_)                              ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BITS_TWPLL_KINT(_X_)                              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )

/* bits definitions for register REG_AON_APB_LTEPLL_CFG1 */
#define BITS_LTEPLL_RES(_X_)                              ( (_X_) << 28 & (BIT(28)|BIT(29)) )
#define BIT_LTEPLL_LOCK_DONE                              ( BIT(27) )
#define BIT_LTEPLL_DIV_S                                  ( BIT(26) )
#define BIT_LTEPLL_MOD_EN                                 ( BIT(25) )
#define BIT_LTEPLL_SDM_EN                                 ( BIT(24) )
#define BITS_LTEPLL_LPF(_X_)                              ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_LTEPLL_REFIN(_X_)                            ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_LTEPLL_IBIAS(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_LTEPLL_N(_X_)                                ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_LTEPLL_CFG2 */
#define BITS_LTEPLL_NINT(_X_)                             ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BITS_LTEPLL_KINT(_X_)                             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )

/* bits definitions for register REG_AON_APB_LVDSDISPLL_CFG1 */
#define BITS_LVDSDISPLL_RES(_X_)                          ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_LVDSDISPLL_LOCK_DONE                          ( BIT(27) )
#define BIT_LVDSDISPLL_DIV_S                              ( BIT(26) )
#define BIT_LVDSDISPLL_MOD_EN                             ( BIT(25) )
#define BIT_LVDSDISPLL_SDM_EN                             ( BIT(24) )
#define BITS_LVDSDISPLL_LPF(_X_)                          ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_LVDSDISPLL_REFIN(_X_)                        ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_LVDSDISPLL_IBIAS(_X_)                        ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_LVDSDISPLL_N(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AON_APB_LVDSDISPLL_CFG2 */
#define BITS_LVDSDISPLL_NINT(_X_)                         ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BITS_LVDSDISPLL_KINT(_X_)                         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )

/* bits definitions for register REG_AON_APB_AON_REG_PROT */
#define BIT_LDSP_CTRL_PROT                                ( BIT(31) )
#define BITS_REG_PROT_VAL(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AON_APB_LDSP_BOOT_EN */
#define BIT_FRC_CLK_LDSP_EN                               ( BIT(1) )
#define BIT_LDSP_BOOT_EN                                  ( BIT(0) )

/* bits definitions for register REG_AON_APB_LDSP_BOOT_VEC */
#define BITS_LDSP_BOOT_VECTOR(_X_)                        (_X_)

/* bits definitions for register REG_AON_APB_LDSP_RST */
#define BIT_LDSP_SYS_SRST                                 ( BIT(1) )
#define BIT_LDSP_CORE_SRST_N                              ( BIT(0) )

/* bits definitions for register REG_AON_APB_LDSP_MTX_CTRL1 */
#define BITS_LDSP_MTX_CTRL1(_X_)                          (_X_)

/* bits definitions for register REG_AON_APB_LDSP_MTX_CTRL2 */
#define BITS_LDSP_MTX_CTRL2(_X_)                          (_X_)

/* bits definitions for register REG_AON_APB_LDSP_MTX_CTRL3 */
#define BITS_LDSP_MTX_CTRL3(_X_)                          (_X_)

/* bits definitions for register REG_AON_APB_AON_CGM_CFG */
#define BITS_PROBE_CKG_DIV(_X_)                           ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_AUX2_CKG_DIV(_X_)                            ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_AUX1_CKG_DIV(_X_)                            ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_AUX0_CKG_DIV(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_PROBE_CKG_SEL(_X_)                           ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_AUX2_CKG_SEL(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_AUX1_CKG_SEL(_X_)                            ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_AUX0_CKG_SEL(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_LACC_MTX_CTRL */
#define BITS_LACC_MTX_CTRL(_X_)                           (_X_)

/* bits definitions for register REG_AON_APB_CORTEX_MTX_CTRL1 */
#define BITS_CORTEX_MTX_CTRL1(_X_)                        (_X_)

/* bits definitions for register REG_AON_APB_CORTEX_MTX_CTRL2 */
#define BITS_CORTEX_MTX_CTRL2(_X_)                        (_X_)

/* bits definitions for register REG_AON_APB_CORTEX_MTX_CTRL3 */
#define BITS_CORTEX_MTX_CTRL3(_X_)                        ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AON_APB_CA5_TCLK_DLY_LEN */
#define BITS_CA5_TCLK_DLY_LEN(_X_)                        ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AON_APB_CCIR_RCVR_CFG */
#define BITS_ANALOG_PLL_RSV(_X_)                          ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_ANALOG_TESTMUX(_X_)                          ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_CCIR_SE                                       ( BIT(1) )
#define BIT_CCIR_IE                                       ( BIT(0) )

/* bits definitions for register REG_AON_APB_PLL_BG_CFG */
#define BITS_PLL_BG_RSV(_X_)                              ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_PLL_BG_RBIAS_EN                               ( BIT(3) )
#define BIT_PLL_BG_PD                                     ( BIT(2) )
#define BIT_PLL_BG_IEXT_IBEN                              ( BIT(1) )
#define BIT_PLL_CON_BG                                    ( BIT(0) )

/* bits definitions for register REG_AON_APB_LVDSDIS_SEL */
#define BITS_LVDSDIS_LOG_SEL(_X_)                         ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_LVDSDIS_DBG_SEL                               ( BIT(0) )

/* bits definitions for register REG_AON_APB_DJTAG_MUX_SEL */
#if defined(CONFIG_MACH_SP9830I)
#define BIT_DjTAG_CODEC_SEL                               (BIT(7))
#endif
#define BIT_DJTAG_AON_SEL                                 ( BIT(6) )
#define BIT_DJTAG_PUB_SEL                                 ( BIT(5) )
#define BIT_DJTAG_CP1_SEL                                 ( BIT(4) )
#define BIT_DJTAG_CP0_SEL                                 ( BIT(3) )
#define BIT_DJTAG_GPU_SEL                                 ( BIT(2) )
#define BIT_DJTAG_MM_SEL                                  ( BIT(1) )
#define BIT_DJTAG_AP_SEL                                  ( BIT(0) )

/* bits definitions for register REG_AON_APB_ARM7_SYS_SOFT_RST */
#define BIT_ARM7_SYS_SOFT_RST                             ( BIT(4) )
#define BIT_ARM7_CORE_SOFT_RST                            ( BIT(0) )

/* bits definitions for register REG_AON_APB_CP1_CP0_ADDR_MSB */
#define BITS_CP1_CP0_ADDR_MSB(_X_)                        ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_AON_DMA_INT_EN */
#define BIT_AON_DMA_INT_ARM7_EN                           ( BIT(6) )
#define BIT_AON_DMA_INT_CP1_DSP_EN                        ( BIT(5) )
#define BIT_AON_DMA_INT_CP1_CA5_EN                        ( BIT(4) )
#define BIT_AON_DMA_INT_CP0_DSP_1_EN                      ( BIT(3) )
#define BIT_AON_DMA_INT_CP0_DSP_0_EN                      ( BIT(2) )
#define BIT_AON_DMA_INT_CP0_ARM9_0_EN                     ( BIT(1) )
#define BIT_AON_DMA_INT_AP_EN                             ( BIT(0) )

/* bits definitions for register REG_AON_APB_EMC_AUTO_GATE_EN */
#define BIT_CP1_PUB_AUTO_GATE_EN                          ( BIT(19) )
#define BIT_CP0_PUB_AUTO_GATE_EN                          ( BIT(18) )
#define BIT_AP_PUB_AUTO_GATE_EN                           ( BIT(17) )
#define BIT_AON_APB_PUB_AUTO_GATE_EN                      ( BIT(16) )
#define BIT_CP1_EMC_AUTO_GATE_EN                          ( BIT(3) )
#define BIT_CP0_EMC_AUTO_GATE_EN                          ( BIT(2) )
#define BIT_AP_EMC_AUTO_GATE_EN                           ( BIT(1) )
#define BIT_CA7_EMC_AUTO_GATE_EN                          ( BIT(0) )

/* bits definitions for register REG_AON_APB_ARM7_CFG_BUS */
#define BIT_ARM7_CFG_BUS_SLEEP                            ( BIT(0) )

/* bits definitions for register REG_AON_APB_RTC4M_0_CFG */
#define BITS_RTC4M0_RSV(_X_)                              ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_RTC4M0_I_C(_X_)                              ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_RTC4M0_CAL_DONE                               ( BIT(6) )
#define BIT_RTC4M0_CAL_START                              ( BIT(5) )
#define BIT_RTC4M0_CHOP_EN                                ( BIT(4) )
#define BIT_RTC4M0_FORCE_EN                               ( BIT(1) )
#define BIT_RTC4M0_AUTO_GATE_EN                           ( BIT(0) )

/* bits definitions for register REG_AON_APB_RTC4M_1_CFG */
#define BITS_RTC4M1_RSV(_X_)                              ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_RTC4M1_I_C(_X_)                              ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_RTC4M1_CAL_DONE                               ( BIT(6) )
#define BIT_RTC4M1_CAL_START                              ( BIT(5) )
#define BIT_RTC4M1_CHOP_EN                                ( BIT(4) )
#define BIT_RTC4M1_FORCE_EN                               ( BIT(1) )
#define BIT_RTC4M1_AUTO_GATE_EN                           ( BIT(0) )

/* bits definitions for register REG_AON_APB_APB_RST2 */
#if defined(CONFIG_MACH_SP9830I)
#define BIT_CODEC_DJTAG_SOFT_RST                          (BIT(7))
#endif
#define BIT_AON_DJTAG_SOFT_RST                            ( BIT(6) )
#define BIT_PUB_DJTAG_SOFT_RST                            ( BIT(5) )
#define BIT_GPU_DJTAG_SOFT_RST                            ( BIT(4) )
#define BIT_MM_DJTAG_SOFT_RST                             ( BIT(3) )
#define BIT_CP1_DJTAG_SOFT_RST                            ( BIT(2) )
#define BIT_CP0_DJTAG_SOFT_RST                            ( BIT(1) )
#define BIT_AP_DJTAG_SOFT_RST                             ( BIT(0) )

/* bits definitions for register REG_AON_APB_AP_WPROT_EN1 */
#define BITS_AP_AWADDR_WPROT_EN1(_X_)                     (_X_)

/* bits definitions for register REG_AON_APB_CP0_WPROT_EN1 */
#define BITS_CP0_AWADDR_WPROT_EN1(_X_)                    (_X_)

/* bits definitions for register REG_AON_APB_CP1_WPROT_EN1 */
#define BITS_CP1_AWADDR_WPROT_EN1(_X_)                    (_X_)

/* bits definitions for register REG_AON_APB_IO_DLY_CTRL */
#define BITS_CLK_CCIR_DLY_SEL(_X_)                        ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CLK_CP1DSP_DLY_SEL(_X_)                      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CLK_CP0DSP_DLY_SEL(_X_)                      ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_AP_WPROT_EN0 */
#define BITS_AP_AWADDR_WPROT_EN0(_X_)                     (_X_)

/* bits definitions for register REG_AON_APB_CP0_WPROT_EN0 */
#define BITS_CP0_AWADDR_WPROT_EN0(_X_)                    (_X_)

/* bits definitions for register REG_AON_APB_CP1_WPROT_EN0 */
#define BITS_CP1_AWADDR_WPROT_EN0(_X_)                    (_X_)

/* bits definitions for register REG_AON_APB_PMU_RST_MONITOR */
#define BITS_PMU_RST_MONITOR(_X_)                         (_X_)

/* bits definitions for register REG_AON_APB_THM_RST_MONITOR */
#define BITS_THM_RST_MONITOR(_X_)                         (_X_)

/* bits definitions for register REG_AON_APB_AP_RST_MONITOR */
#define BITS_AP_RST_MONITOR(_X_)                          (_X_)

/* bits definitions for register REG_AON_APB_CA7_RST_MONITOR */
#define BITS_CA7_RST_MONITOR(_X_)                         (_X_)

/* bits definitions for register REG_AON_APB_BOND_OPT0 */
#define BITS_BOND_OPTION0(_X_)                            (_X_)

/* bits definitions for register REG_AON_APB_BOND_OPT1 */
#define BITS_BOND_OPTION1(_X_)                            (_X_)

/* bits definitions for register REG_AON_APB_RES_REG0 */
#define BITS_RES_REG0(_X_)                                (_X_)

/* bits definitions for register REG_AON_APB_RES_REG1 */
#define BITS_RES_REG1(_X_)                                (_X_)

/* bits definitions for register REG_AON_APB_AON_QOS_CFG */
#define BITS_QOS_R_GPU(_X_)                               ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_QOS_W_GPU(_X_)                               ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_QOS_R_GSP(_X_)                               ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_QOS_W_GSP(_X_)                               ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_BB_LDO_CAL_START */
#define BIT_BB_LDO_CAL_START                              ( BIT(0) )

/* bits definitions for register REG_AON_APB_AON_MTX_PROT_CFG */
#define BITS_HPROT_DMAW(_X_)                              ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_HPROT_DMAR(_X_)                              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_LVDS_CFG */
#define BITS_LVDSDIS_TXCLKDATA(_X_)                       ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)) )
#define BITS_LVDSDIS_TXCOM(_X_)                           ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_LVDSDIS_TXSLEW(_X_)                          ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_LVDSDIS_TXSW(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_LVDSDIS_TXRERSER(_X_)                        ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LVDSDIS_PRE_EMP(_X_)                         ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_LVDSDIS_TXPD                                  ( BIT(0) )

/* bits definitions for register REG_AON_APB_PLL_LOCK_OUT_SEL */
#define BIT_SLEEP_PLLLOCK_SEL                             ( BIT(7) )
#define BITS_PLL_LOCK_SEL(_X_)                            ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_SLEEP_DBG_SEL(_X_)                           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AON_APB_RTC4M_RC_VAL */
#define BIT_RTC4M1_RC_SEL                                 ( BIT(31) )
#define BITS_RTC4M1_RC_VAL(_X_)                           ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)) )
#define BIT_RTC4M0_RC_SEL                                 ( BIT(15) )
#define BITS_RTC4M0_RC_VAL(_X_)                           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)) )

/* bits definitions for register REG_AON_APB_AON_APB_RSV */
#define BITS_AON_APB_RSV(_X_)                             (_X_)

/* bits definitions for register REG_AON_APB_AON_CHIP_ID */
#define BITS_AON_CHIP_ID(_X_)                             (_X_)

#endif
