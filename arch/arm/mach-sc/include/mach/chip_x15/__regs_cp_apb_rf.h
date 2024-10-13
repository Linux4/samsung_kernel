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


#ifndef __H_REGS_CP_APB_RF_HEADFILE_H__
#define __H_REGS_CP_APB_RF_HEADFILE_H__ __FILE__

#define	REGS_CP_APB_RF

/* registers definitions for CP_APB_RF */
#define REG_CP_APB_RF_APB_EB0_STS                         SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0008)
#define REG_CP_APB_RF_APB_RST0_STS                        SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0014)
#define REG_CP_APB_RF_APB_MCU_RST                         SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0018)
#define REG_CP_APB_RF_APB_CLK_SEL0                        SCI_ADDR(REGS_CP_APB_RF_BASE, 0x001C)
#define REG_CP_APB_RF_APB_CLK_DIV0                        SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0020)
#define REG_CP_APB_RF_APB_ARCH_EB                         SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0024)
#define REG_CP_APB_RF_APB_MISC_CTL0                       SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0028)
#define REG_CP_APB_RF_APB_MISC_CTL1                       SCI_ADDR(REGS_CP_APB_RF_BASE, 0x002C)
#define REG_CP_APB_RF_APB_PIN_SEL                         SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0040)
#define REG_CP_APB_RF_APB_SLP_CTL                         SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0044)
#define REG_CP_APB_RF_APB_WSYS_STS                        SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0048)
#define REG_CP_APB_RF_APB_SLP_STS                         SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0050)
#define REG_CP_APB_RF_APB_ROM_PD_CTL                      SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0054)
#define REG_CP_APB_RF_APB_BUS_CTL0                        SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0058)
#define REG_CP_APB_RF_APB_DSP_INT_CLR                     SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0074)
#define REG_CP_APB_RF_APB_MISC_INT_STS                    SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0078)
#define REG_CP_APB_RF_APB_HWRST                           SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0080)
#define REG_CP_APB_RF_APB_ARM_BOOT_ADDR                   SCI_ADDR(REGS_CP_APB_RF_BASE, 0x0084)



/* bits definitions for register REG_CP_APB_RF_APB_EB0_STS */
#define BIT_ADA_EB                                        ( BIT(18) )
#define BIT_RFFE_EB                                       ( BIT(17) )
#define BIT_EPT_EB                                        ( BIT(16) )
#define BIT_CP_GPIO_EB                                       ( BIT(15) )
#define BIT_TMR_RTC_EB                                    ( BIT(14) )
#define BIT_TMR_EB                                        ( BIT(13) )
#define BIT_SYSTMR_RTC_EB                                 ( BIT(12) )
#define BIT_SYSTMR_EB                                     ( BIT(11) )
#define BIT_CP_IIS3_EB                                       ( BIT(10) )
#define BIT_CP_IIS2_EB                                       ( BIT(9) )
#define BIT_CP_IIS1_EB                                       ( BIT(8) )
#define BIT_CP_IIS0_EB                                       ( BIT(7) ) #define BIT_SIM1_EB                                       ( BIT(5) )
#define BIT_CP_SIM0_EB                                       ( BIT(4) )
#define BIT_CP_UART1_EB                                      ( BIT(3) )
#define BIT_CP_UART0_EB                                      ( BIT(2) )
#define BIT_WDG_RTC_EB                                    ( BIT(1) )
#define BIT_WDG_EB                                        ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_RST0_STS */
#define BIT_ADA_TX_SOFT_RST                               ( BIT(18) )
#define BIT_ADA_RX_SOFT_RST                               ( BIT(17) )
#define BIT_ADA_SOFT_RST                                  ( BIT(16) )
#define BIT_RFFE_SOFT_RST                                 ( BIT(15) )
#define BIT_MCU_DSP_RST                                   ( BIT(14) )
#define BIT_EPT_SOFT_RST                                  ( BIT(13) )
#define BIT_CP_GPIO_SOFT_RST                                 ( BIT(12) )
#define BIT_TMR_SOFT_RST                                  ( BIT(11) )
#define BIT_SYSTMR_SOFT_RST                               ( BIT(10) )
#define BIT_CP_IIS3_SOFT_RST                                 ( BIT(9) )
#define BIT_CP_IIS2_SOFT_RST                                 ( BIT(8) )
#define BIT_CP_IIS1_SOFT_RST                                 ( BIT(7) )
#define BIT_CP_IIS0_SOFT_RST                                 ( BIT(6) )
#define BIT_SIM2_SOFT_RST                                 ( BIT(5) )
#define BIT_SIM1_SOFT_RST                                 ( BIT(4) )
#define BIT_CP_SIM0_SOFT_RST                                 ( BIT(3) )
#define BIT_CP_UART1_SOFT_RST                                ( BIT(2) )
#define BIT_CP_UART0_SOFT_RST                                ( BIT(1) )
#define BIT_WDG_SOFT_RST                                  ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_MCU_RST */
#define BIT_MCU_SOFT_RST_SET                              ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_CLK_SEL0 */
#define BITS_CLK_IIS3_SEL(_X_)                            ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_CLK_IIS2_SEL(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_CLK_IIS1_SEL(_X_)                            ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_CLK_IIS0_SEL(_X_)                            ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_CLK_UART1_SEL(_X_)                           ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_CLK_UART0_SEL(_X_)                           ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CP_APB_RF_APB_CLK_DIV0 */
#define BITS_CLK_IIS2_DIV(_X_)                            ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BITS_CLK_IIS1_DIV(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)) )
#define BITS_CLK_IIS0_DIV(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_CLK_UART1_DIV(_X_)                           ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_CLK_UART0_DIV(_X_)                           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_CP_APB_RF_APB_ARCH_EB */
#define BITS_CLK_IIS3_DIV(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_RTC_ARCH_EB                                   ( BIT(1) )
#define BIT_APB_ARCH_EB                                   ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_MISC_CTL0 */
#define BIT_ALL_CLK_EN                                    ( BIT(28) )
#define BIT_DMA_LSLP_EN                                   ( BIT(27) )
#define BIT_WAKEUP_XTL_EN_3G                              ( BIT(26) )
#define BIT_WAKEUP_XTL_EN_2G                              ( BIT(25) )
#define BIT_ARM_JTAG_EN                                   ( BIT(24) )
#define BITS_ARM_FRC_STOP(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_CP_APB_RF_APB_MISC_CTL1 */
#define BITS_BUFON_CTRL(_X_)                              ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BIT_SIM2_CLK_POLARITY                             ( BIT(3) )
#define BIT_SIM1_CLK_POLARITY                             ( BIT(2) )
#define BIT_SIM0_CLK_POLARITY                             ( BIT(1) )
#define BIT_ROM_CLK_EN                                    ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_PIN_SEL */
#define BIT_DJTAG_PIN_IN_SEL                              ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_SLP_CTL */
#define BIT_APB_PERI_FRC_ON                               ( BIT(20) )
#define BIT_APB_PERI_FRC_SLP                              ( BIT(16) )
#define BIT_MCU_XTLEN_AUTOPD_EN                           ( BIT(12) )
#define BIT_CHIP_SLP_ARM_CLR                              ( BIT(4) )
#define BIT_MCU_FORCE_WSYS_LT_STOP                        ( BIT(2) )
#define BIT_MCU_FORCE_WSYS_STOP                           ( BIT(1) )
#define BIT_MCU_FORCE_DEEP_SLEEP                          ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_WSYS_STS */
#define BITS_DEEP_SLP_DBG(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_WSYS_STATUS(_X_)                             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_CP_APB_RF_APB_SLP_STS */
#define BIT_DSP_MAHB_SLEEP_EN                             ( BIT(28) )
#define BIT_MCU_PERI_STOP                                 ( BIT(27) )
#define BIT_DSP_PERI_STOP                                 ( BIT(26) )
#define BIT_ECC_CKG_EN                                    ( BIT(25) )
#define BIT_QBC_CKG_EN                                    ( BIT(24) )
#define BIT_DSP_DPLL_EN                                   ( BIT(23) )
#define BIT_DSP_TDPLL_EN                                  ( BIT(22) )
#define BIT_DSP_MPLL_EN                                   ( BIT(21) )
#define BIT_STC_TMR_AUTOPD_XTL_EN                         ( BIT(20) )
#define BIT_RFT_TMR_AUTOPD_XTL_EN                         ( BIT(19) )
#define BIT_STC_TMR_AUTOPD_RF_EN                          ( BIT(18) )
#define BIT_RFT_TMR_AUTOPD_RF_EN                          ( BIT(17) )
#define BIT_CHIP_SLEEP_REC_ARM                            ( BIT(16) )
#define BIT_DSP_CORE_STOP                                 ( BIT(15) )
#define BIT_DSP_MTX_STOP                                  ( BIT(14) )
#define BIT_DSP_AHB_STOP                                  ( BIT(13) )
#define BIT_DSP_SYS_STOP                                  ( BIT(12) )
#define BIT_DSP_DEEP_STOP                                 ( BIT(11) )
#define BIT_ASHB_DSPTOARM_EN                              ( BIT(10) )
#define BIT_ASHB_ARMTODSP_VALID                           ( BIT(9) )
#define BIT_EMC_STOP_CH3                                  ( BIT(8) )
#define BIT_EMC_STOP_CH4                                  ( BIT(7) )
#define BIT_EMC_STOP_CH5                                  ( BIT(6) )
#define BIT_FRC_WAKE_EN                                   ( BIT(5) )
#define BIT_TMR_WAKE_AFC                                  ( BIT(4) )
#define BIT_SCH_SLEEP                                     ( BIT(3) )
#define BIT_DSP_STOP                                      ( BIT(2) )
#define BIT_MCU_STOP                                      ( BIT(1) )
#define BIT_EMC_STOP                                      ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_ROM_PD_CTL */
#define BIT_ROM_FORCE_ON                                  ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_BUS_CTL0 */
#define BIT_ADA_CTRL_SEL                                  ( BIT(6) )
#define BIT_RFFE_CTRL_SEL                                 ( BIT(5) )
#define BIT_IIS3_CTRL_SEL                                 ( BIT(4) )
#define BIT_IIS2_CTRL_SEL                                 ( BIT(3) )
#define BIT_IIS1_CTRL_SEL                                 ( BIT(2) )
#define BIT_IIS0_CTRL_SEL                                 ( BIT(1) )
#define BIT_UART1_CTRL_SEL                                ( BIT(0) )

/* bits definitions for register REG_CP_APB_RF_APB_DSP_INT_CLR */
#define BIT_RFT_INT_CLR                                   ( BIT(2) )

/* bits definitions for register REG_CP_APB_RF_APB_MISC_INT_STS */
#define BIT_RFT_INT                                       ( BIT(2) )

/* bits definitions for register REG_CP_APB_RF_APB_HWRST */
#define BITS_HWRST_REG(_X_)                               (_X_)

/* bits definitions for register REG_CP_APB_RF_APB_ARM_BOOT_ADDR */
#define BITS_ARMBOOT_ADDR(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

#endif
