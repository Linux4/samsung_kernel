/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef __REGS_GLB_H__
#define __REGS_GLB_H__

#ifndef __SCI_GLB_REGS_H__
#error  "Don't include this file directly, include <mach/sci_glb_regs.h>"
#endif

#define REGS_GLB

/* registers definitions for controller REGS_GLB */
#define REG_GLB_MCU_SOFT_RST            SCI_ADDR(REGS_GLB_BASE, 0x0004)
#define REG_GLB_GEN0                    SCI_ADDR(REGS_GLB_BASE, 0x0008)
#define REG_GLB_PCTRL                   SCI_ADDR(REGS_GLB_BASE, 0x000c)
#define REG_GLB_GEN1                    SCI_ADDR(REGS_GLB_BASE, 0x0018)
#define REG_GLB_GEN3                    SCI_ADDR(REGS_GLB_BASE, 0x001c)
#define REG_GLB_HWRST                   SCI_ADDR(REGS_GLB_BASE, 0x0020)
#define REG_GLB_M_PLL_CTL0              SCI_ADDR(REGS_GLB_BASE, 0x0024)
#define REG_GLB_PINCTL                  SCI_ADDR(REGS_GLB_BASE, 0x0028)
#define REG_GLB_GEN2                    SCI_ADDR(REGS_GLB_BASE, 0x002c)
#define REG_GLB_ARMBOOT                 SCI_ADDR(REGS_GLB_BASE, 0x0030)
#define REG_GLB_STC_DSP_ST              SCI_ADDR(REGS_GLB_BASE, 0x0034)
#define REG_GLB_TD_PLL_CTL              SCI_ADDR(REGS_GLB_BASE, 0x003c)
#define REG_GLB_D_PLL_CTL               SCI_ADDR(REGS_GLB_BASE, 0x0040)
#define REG_GLB_BUSCLK                  SCI_ADDR(REGS_GLB_BASE, 0x0044)
#define REG_GLB_ARCH                    SCI_ADDR(REGS_GLB_BASE, 0x0048)
#define REG_GLB_SOFT_RST                SCI_ADDR(REGS_GLB_BASE, 0x004c)
#define REG_GLB_G_PLL_CTL               SCI_ADDR(REGS_GLB_BASE, 0x0050)
#define REG_GLB_NFCMEMDLY               SCI_ADDR(REGS_GLB_BASE, 0x0058)
#define REG_GLB_CLKDLY                  SCI_ADDR(REGS_GLB_BASE, 0x005c)
#define REG_GLB_GEN4                    SCI_ADDR(REGS_GLB_BASE, 0x0060)
#define REG_GLB_A_PLLMN                 SCI_ADDR(REGS_GLB_BASE, 0x0064)
#define REG_GLB_POWCTL0                 SCI_ADDR(REGS_GLB_BASE, 0x0068)
#define REG_GLB_POWCTL1                 SCI_ADDR(REGS_GLB_BASE, 0x006c)
#define REG_GLB_PLL_SCR                 SCI_ADDR(REGS_GLB_BASE, 0x0070)
#define REG_GLB_CLK_EN                  SCI_ADDR(REGS_GLB_BASE, 0x0074)
#define REG_GLB_CLK26M_ANA_CTL          SCI_ADDR(REGS_GLB_BASE, 0x0078)
#define REG_GLB_CLK_GEN5                SCI_ADDR(REGS_GLB_BASE, 0x007c)
#define REG_GLB_DDR_PHY_RETENTION       SCI_ADDR(REGS_GLB_BASE, 0x0080)
#define REG_GLB_MM_PWR_CTL              SCI_ADDR(REGS_GLB_BASE, 0x0084)
#define REG_GLB_CEVA_L1RAM_PWR_CTL      SCI_ADDR(REGS_GLB_BASE, 0x0088)
#define REG_GLB_GSM_PWR_CTL             SCI_ADDR(REGS_GLB_BASE, 0x008c)
#define REG_GLB_TD_PWR_CTL              SCI_ADDR(REGS_GLB_BASE, 0x0090)
#define REG_GLB_PERI_PWR_CTL            SCI_ADDR(REGS_GLB_BASE, 0x0094)
#define REG_GLB_ARM_SYS_PWR_CTL         SCI_ADDR(REGS_GLB_BASE, 0x009c)
#define REG_GLB_G3D_PWR_CTL             SCI_ADDR(REGS_GLB_BASE, 0x00a0)
#define REG_GLB_CHIP_DSLEEP_STAT        SCI_ADDR(REGS_GLB_BASE, 0x00a4)
#define REG_GLB_PWR_CTRL_NUM1           SCI_ADDR(REGS_GLB_BASE, 0x00a8)
#define REG_GLB_PWR_CTRL_NUM2           SCI_ADDR(REGS_GLB_BASE, 0x00ac)
#define REG_GLB_PWR_CTRL_NUM3           SCI_ADDR(REGS_GLB_BASE, 0x00b0)
#define REG_GLB_PWR_CTRL_NUM4           SCI_ADDR(REGS_GLB_BASE, 0x00b4)
#define REG_GLB_PWR_CTRL_NUM5           SCI_ADDR(REGS_GLB_BASE, 0x00b8)
#define REG_GLB_ARM9_SYS_PWR_CTL        SCI_ADDR(REGS_GLB_BASE, 0x00bc)
#define REG_GLB_DMA_CTRL                SCI_ADDR(REGS_GLB_BASE, 0x00c0)
#define REG_GLB_DMA_AP_CP_SEL           SCI_ADDR(REGS_GLB_BASE, 0x00c4)
#define REG_GLB_SLEEP_INT_AP_SEL        SCI_ADDR(REGS_GLB_BASE, 0x00c8)
#define REG_GLB_SLEEP_INT_CP_SEL        SCI_ADDR(REGS_GLB_BASE, 0x00cc)
#define REG_GLB_TEST_CLK_CTRL           SCI_ADDR(REGS_GLB_BASE, 0x00d0)
#define REG_GLB_TP_DLYC_LEN             SCI_ADDR(REGS_GLB_BASE, 0x00d4)
#define REG_GLB_MIPI_PHY_CTRL           SCI_ADDR(REGS_GLB_BASE, 0x00d8)
#define REG_GLB_BOND_OPTION             SCI_ADDR(REGS_GLB_BASE, 0x0100)

/* bits definitions for register REG_GLB_MCU_SOFT_RST */
/* MCU soft reset the whole MCU sub-system, processor core, AHB and APB
 * this bit will be self-cleared to zero after set
 */
#define BIT_MCU_SOFT_RST                ( BIT(0) )

/* bits definitions for register REG_GLB_GEN0 */
#define BIT_IC3_EB                      ( BIT(31) )
#define BIT_IC2_EB                      ( BIT(30) )
#define BIT_IC1_EB                      ( BIT(29) )
#define BIT_RTC_TMR_EB                  ( BIT(28) )
#define BIT_RTC_SYST0_EB                ( BIT(27) )
#define BIT_RTC_KPD_EB                  ( BIT(26) )
#define BIT_IIS1_EB                     ( BIT(25) )
#define BIT_RTC_EIC_EB                  ( BIT(24) )
#define BIT_UART2_EB                    ( BIT(22) )
#define BIT_UART1_EB                    ( BIT(21) )
#define BIT_UART0_EB                    ( BIT(20) )
#define BIT_SYST0_EB                    ( BIT(19) )
#define BIT_SPI1_EB                     ( BIT(18) )
#define BIT_SPI0_EB                     ( BIT(17) )
#define BIT_SIM1_EB                     ( BIT(16) )
#define BIT_EPT_EB                      ( BIT(15) )
#define BIT_CCIR_MCLK_EN                ( BIT(14) )
#define BIT_PINREG_EB                   ( BIT(13) )
#define BIT_IIS0_EB                     ( BIT(12) )
/* MCU soft reset DSP Z-bus Accelerators, it will be self-cleared to zero after set
 */
#define BIT_MCU_DSP_RST                 ( BIT(10) )
#define BIT_EIC_EB                      ( BIT(9) )
#define BIT_KPD_EB                      ( BIT(8) )
#define BIT_EFUSE_EB                    ( BIT(7) )
#define BIT_ADI_EB                      ( BIT(6) )
#define BIT_GPIO_EB                     ( BIT(5) )
#define BIT_I2C0_EB                     ( BIT(4) )
#define BIT_SIM0_EB                     ( BIT(3) )
#define BIT_TMR_EB                      ( BIT(2) )
#define BIT_SPI2_EB                     ( BIT(1) )
#define BIT_UART3_EB                    ( BIT(0) )

/* bits definitions for register REG_GLB_PCTRL */
#define BIT_IIS0_CTL_SEL                ( BIT(31) )
#define BIT_IIS1_CTL_SEL                ( BIT(30) )
#define BITS_CLK_AUX1_DIV(_x_)          ( (_x_) << 22 & (BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BIT_GPLL_CNT_DONE               ( BIT(17) )
#define BIT_MCU_GPLL_EN                 ( BIT(16) )
#define BIT_ROM_FORCE_ON                ( BIT(10) )
/* All clock gatings will be invalid ,ad then all clock enable, for debug use */
#define BIT_CLK_ALL_EN                  ( BIT(9) )
/* Owner selection for UART1, <0: ARM control, 1: DSP control> */
#define BIT_UART1_CTL_SEL               ( BIT(8) )
/* Flag indicating MPLL is stable, active high. Only for SW debug. */
#define BIT_MPLL_CNT_DONE               ( BIT(7) )
#define BIT_TDPLL_CNT_DONE              ( BIT(6) )
#define BIT_DPLL_CNT_DONE               ( BIT(5) )
#define BIT_ARM_JTAG_EN                 ( BIT(4) )
#define BIT_MCU_DPLL_EN                 ( BIT(3) )
#define BIT_MCU_TDPLL_EN                ( BIT(2) )
#define BIT_MCU_MPLL_EN                 ( BIT(1) )
/* MCU force deepsleep. for debug use. */
#define BIT_MCU_FORECE_DEEP_SLEEP       ( BIT(0) )

/* bits definitions for register REG_GLB_GEN1 */
#define BIT_AUD_CTL_SEL                 ( BIT(22) )
#define BIT_AUDIF_AUTO_EN               ( BIT(21) )
#define BITS_AUDIF_SEL(_x_)             ( (_x_) << 19 & (BIT(19)|BIT(20)) )
#define BIT_RTC_ARCH_EB                 ( BIT(18) )
#define BIT_AUD_CLK_SEL                 ( BIT(17) )
#define BIT_VBC_EN                      ( BIT(14) )
#define BIT_AUD_TOP_EB                  ( BIT(13) )
#define BIT_AUD_IF_EB                   ( BIT(12) )
#define BIT_CLK_AUX1_EN                 ( BIT(11) )
#define BIT_CLK_AUX0_EN                 ( BIT(10) )
#define BIT_MPLL_CTL_WE                 ( BIT(9) )
#define BITS_CLK_AUX0_DIV(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_GLB_GEN3 */
#define BITS_CCIR_MCLK_DIV(_x_)         ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_JTAG_DAISY_EN               ( BIT(23) )
#define BITS_CLK_UART3_DIV(_x_)         ( (_x_) << 18 & (BIT(18)|BIT(19)|BIT(20)) )
#define BITS_CLK_UART3_SEL(_x_)         ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_CLK_IIS1_DIV(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CLK_SPI2_DIV(_x_)          ( (_x_) << 5 & (BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CLK_SPI2_SEL(_x_)          ( (_x_) << 3 & (BIT(3)|BIT(4)) )

/* bits definitions for register REG_GLB_HWRST */
#define BITS_HWRST(_x_)                 ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

#define SHFT_HWRST                      ( 8 )
#define MASK_HWRST                      ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

/* bits definitions for register REG_GLB_M_PLL_CTL0 */
#define BITS_MPLL_REFIN(_x_)            ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_MPLL_LPF(_x_)              ( (_x_) << 13 & (BIT(13)|BIT(14)|BIT(15)) )
#define BITS_MPLL_IBIAS(_x_)            ( (_x_) << 11 & (BIT(11)|BIT(12)) )
#define BITS_MPLL_N(_x_)                ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

#define SHFT_MPLL_REFIN                 ( 16 )
#define MASK_MPLL_REFIN                 ( BIT(16)|BIT(17) )

#define SHFT_MPLL_N                     ( 0 )
#define MASK_MPLL_N                     ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10) )

/* bits definitions for register REG_GLB_PINCTL */
#define BITS_AJTAG_PIN_IN_SEL(_x_)      ( (_x_) << 19 & (BIT(19)|BIT(20)) )
#define BITS_CCIRCK_PIN_IN_SEL(_x_)     ( (_x_) << 17 & (BIT(17)|BIT(18)) )
#define BIT_CP_VBC_BYPASS_EN            ( BIT(16) )
#define BIT_FM_VBC_BYPASS_EN            ( BIT(15) )
#define BIT_UART_LOOP_SEL               ( BIT(14) )
#define BIT_PCM_LOOP_SEL                ( BIT(13) )
#define BIT_DJTAG_PIN_IN_SEL            ( BIT(12) )
#define BIT_SPI1_PIN_IN_SEL             ( BIT(11) )
#define BIT_FMARK_POL_INV               ( BIT(6) )
#define BIT_SIM1_PIN_IN_SEL             ( BIT(5) )
#define BIT_SIM0_PIN_IN_SEL             ( BIT(4) )

/* bits definitions for register REG_GLB_GEN2 */
#define BITS_CLK_IIS0_DIV(_x_)          ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_CLK_SPI0_DIV(_x_)          ( (_x_) << 21 & (BIT(21)|BIT(22)|BIT(23)) )
#define BITS_CLK_GPU_AXI_DIV(_x_)       ( (_x_) << 14 & (BIT(14)|BIT(15)|BIT(16)) )
#define BITS_CLK_SPI1_DIV(_x_)          ( (_x_) << 11 & (BIT(11)|BIT(12)|BIT(13)) )
#define BITS_CLK_NFC_DIV(_x_)           ( (_x_) << 6 & (BIT(6)|BIT(7)|BIT(8)) )
#define BITS_CLK_NFC_SEL(_x_)           ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_CLK_GPU_AXI_SEL(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_GLB_ARMBOOT */
#define BITS_ARMBOOT_ADDR(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

#define SHFT_ARMBOOT_ADDR               ( 0 )
#define MASK_ARMBOOT_ADDR               ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

/* bits definitions for register REG_GLB_STC_DSP_ST */
#define BITS_STC_DSP_STATE(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_GLB_TD_PLL_CTL */
#define BIT_TDPLL_DIV2OUT_FORCE_PD      ( BIT(11) )
#define BIT_TDPLL_DIV3OUT_FORCE_PD      ( BIT(10) )
#define BIT_TDPLL_DIV4OUT_FORCE_PD      ( BIT(9) )
#define BIT_TDPLL_DIV5OUT_FORCE_PD      ( BIT(8) )
#define BITS_TDPLL_REFIN(_x_)           ( (_x_) << 5 & (BIT(5)|BIT(6)) )
#define BITS_TDPLL_LPF(_x_)             ( (_x_) << 2 & (BIT(2)|BIT(3)|BIT(4)) )
#define BITS_TDPLL_IBIAS(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_GLB_D_PLL_CTL */
#define BITS_DPLL_REFIN(_x_)            ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_DPLL_LPF(_x_)              ( (_x_) << 13 & (BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DPLL_IBIAS(_x_)            ( (_x_) << 11 & (BIT(11)|BIT(12)) )
#define BITS_DPLL_N(_x_)                ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

#define SHFT_DPLL_N                     ( 0 )
#define MASK_DPLL_N                     ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10) )

/* bits definitions for register REG_GLB_BUSCLK */
#define BIT_ARM_VB_SEL                  ( BIT(28) )
#define BIT_ARM_VBC_ACC_CP              ( BIT(27) )
#define BIT_ARM_VBC_ACC                 ( BIT(26) )
#define BITS_PWRON_DLY_CTRL(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)) )
#define BIT_ARM_VBC_ANAON               ( BIT(6) )
#define BIT_ARM_VBC_AD1ON               ( BIT(5) )
#define BIT_ARM_VBC_AD0ON               ( BIT(4) )
#define BIT_ARM_VBC_DA1ON               ( BIT(3) )
#define BIT_ARM_VBC_DA0ON               ( BIT(2) )
#define BIT_VBC_ARM_RST                 ( BIT(1) )
#define BIT_VBDA_IIS_DATA_SEL           ( BIT(0) )

/* bits definitions for register REG_GLB_ARCH */
#define BIT_ARCH_EB                     ( BIT(10) )

/* bits definitions for register REG_GLB_SOFT_RST */
#define BIT_SPI2_RST                    ( BIT(31) )
#define BIT_UART3_RST                   ( BIT(30) )
#define BIT_EIC_RST                     ( BIT(29) )
#define BIT_EFUSE_RST                   ( BIT(28) )
#define BIT_PWM3_RST                    ( BIT(27) )
#define BIT_PWM2_RST                    ( BIT(26) )
#define BIT_PWM1_RST                    ( BIT(25) )
#define BIT_PWM0_RST                    ( BIT(24) )
#define BIT_ADI_RST                     ( BIT(22) )
#define BIT_GPIO_RST                    ( BIT(21) )
#define BIT_PINREG_RST                  ( BIT(20) )
#define BIT_SYST0_RST                   ( BIT(19) )
#define BIT_VBC_RST                     ( BIT(18) )
#define BIT_IIS1_RST                    ( BIT(17) )
#define BIT_IIS0_RST                    ( BIT(16) )
#define BIT_SPI1_RST                    ( BIT(15) )
#define BIT_SPI0_RST                    ( BIT(14) )
#define BIT_UART2_RST                   ( BIT(13) )
#define BIT_UART1_RST                   ( BIT(12) )
#define BIT_UART0_RST                   ( BIT(11) )
#define BIT_EPT_RST                     ( BIT(10) )
#define BIT_AUD_IF_RST                  ( BIT(9) )
#define BIT_TMR_RST                     ( BIT(8) )
#define BIT_AUD_TOP_RST                 ( BIT(7) )
#define BIT_SIM1_RST                    ( BIT(6) )
#define BIT_SIM0_RST                    ( BIT(5) )
#define BIT_I2C3_RST                    ( BIT(4) )
#define BIT_I2C2_RST                    ( BIT(3) )
#define BIT_I2C1_RST                    ( BIT(2) )
#define BIT_KPD_RST                     ( BIT(1) )
#define BIT_I2C0_RST                    ( BIT(0) )

/* bits definitions for register REG_GLB_G_PLL_CTL */
#define BITS_GPLL_REFIN(_x_)            ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_GPLL_LPF(_x_)              ( (_x_) << 13 & (BIT(13)|BIT(14)|BIT(15)) )
#define BITS_GPLL_IBIAS(_x_)            ( (_x_) << 11 & (BIT(11)|BIT(12)) )
#define BITS_GPLL_N(_x_)                ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

#define SHFT_GPLL_N                     ( 0 )
#define MASK_GPLL_N                     ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10) )

/* bits definitions for register REG_GLB_NFCMEMDLY */
#define BITS_NFC_MEM_DLY(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )

/* bits definitions for register REG_GLB_CLKDLY */
#define BITS_CLK_SPI1_SEL(_x_)          ( (_x_) << 30 & (BIT(30)|BIT(31)) )
#define BIT_CLK_ADI_EN_ARM              ( BIT(29) )
#define BIT_CLK_ADI_SEL                 ( BIT(28) )
#define BITS_CLK_SPI0_SEL(_x_)          ( (_x_) << 26 & (BIT(26)|BIT(27)) )
#define BITS_CLK_UART2_SEL(_x_)         ( (_x_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_CLK_UART1_SEL(_x_)         ( (_x_) << 22 & (BIT(22)|BIT(23)) )
#define BITS_CLK_UART0_SEL(_x_)         ( (_x_) << 20 & (BIT(20)|BIT(21)) )
#define BITS_CLK_CCIR_DLY_SEL(_x_)      ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_CLK_APB_SEL(_x_)           ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DSP_STATUS(_x_)            ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_MCU_VBC_RST                 ( BIT(2) )
#define BIT_CHIP_SLEEP_REC_ARM          ( BIT(1) )
#define BIT_CHIP_SLP_ARM_CLR            ( BIT(0) )

/* bits definitions for register REG_GLB_GEN4 */
#define BIT_XTLBUF_WAIT_SEL             ( BIT(31) )
#define BIT_PLL_WAIT_SEL                ( BIT(30) )
#define BIT_XTL_WAIT_SEL                ( BIT(29) )
#define BITS_ARM_XTLBUF_WAIT(_x_)       ( (_x_) << 21 & (BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)) )
#define BITS_ARM_PLL_WAIT(_x_)          ( (_x_) << 13 & (BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_ARM_XTL_WAIT(_x_)          ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_CLK_LCDC_DIV(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_GLB_A_PLLMN */
#define BITS_A_PLLMN(_x_)               ( (_x_) << 0 )

/* bits definitions for register REG_GLB_POWCTL0 */
#define BITS_ARM_PWR_ON_DLY(_x_)        ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_ARM_SLP_POWOFF_AUTO_EN      ( BIT(23) )
#define BIT_ARM_PCELL_SWAP              ( BIT(16) )
#define BITS_ARM_ISO_ON_NUM(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ARM_ISO_OFF_NUM(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_ARM_PWR_ON_DLY             ( 24 )
#define MASK_ARM_PWR_ON_DLY             ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_ARM_ISO_ON_NUM             ( 8 )
#define MASK_ARM_ISO_ON_NUM             ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_ARM_ISO_OFF_NUM            ( 0 )
#define MASK_ARM_ISO_OFF_NUM            ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_POWCTL1 */
#define BIT_DSP_ROM_FORCE_PD            ( BIT(5) )
#define BIT_MCU_ROM_FORCE_PD            ( BIT(4) )
#define BIT_DSP_ROM_SLP_PD_EN           ( BIT(2) )
#define BIT_MCU_ROM_SLP_PD_EN           ( BIT(0) )

/* bits definitions for register REG_GLB_PLL_SCR */
#define BITS_CLK_DCAMMIPIPLL_SEL(_x_)   ( (_x_) << 22 & (BIT(22)|BIT(23)) )
#define BITS_CLK_CCIRPLL_SEL(_x_)       ( (_x_) << 20 & (BIT(20)|BIT(21)) )
#define BITS_CLK_CCIRMCLKPLL_SEL(_x_)   ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_CLK_IIS1PLL_SEL(_x_)       ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_CLK_AUX1PLL_SEL(_x_)       ( (_x_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_CLK_AUX0PLL_SEL(_x_)       ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_CLK_IIS0PLL_SEL(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_CLK_LCDPLL_SEL(_x_)        ( (_x_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_CLK_DCAMPLL_SEL(_x_)       ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_CLK_VSPPLL_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )

/* bits definitions for register REG_GLB_CLK_EN */
#define BIT_CLK_PWM3_SEL                ( BIT(28) )
#define BIT_CLK_PWM2_SEL                ( BIT(27) )
#define BIT_CLK_PWM1_SEL                ( BIT(26) )
#define BIT_CLK_PWM0_SEL                ( BIT(25) )
#define BIT_PWM3_EB                     ( BIT(24) )
#define BIT_PWM2_EB                     ( BIT(23) )
#define BIT_PWM1_EB                     ( BIT(22) )
#define BIT_PWM0_EB                     ( BIT(21) )
#define BIT_APB_PERI_FRC_ON             ( BIT(20) )
#define BIT_APB_PERI_FRC_SLP            ( BIT(19) )
#define BIT_MCU_XTLEN_AUTOPD_EN         ( BIT(18) )
#define BITS_BUFON_CTRL(_x_)            ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BIT_CLK_TDCAL_EN                ( BIT(9) )
#define BIT_CLK_TDFIR_EN                ( BIT(7) )

/* bits definitions for register REG_GLB_CLK26M_ANA_CTL */
#define BITS_REC_CLK26MHZ_RESERVE(_x_)  ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_REC_CLK26MHZ_CUR_SEL        ( BIT(4) )
#define BIT_REC_CLK26MHZ_BUF_AUTO_EN    ( BIT(3) )
#define BIT_REC_CLK26MHZ_BUF_FORCE_PD   ( BIT(2) )
#define BIT_CLK26M_ANA_SEL              ( BIT(1) )
#define BIT_CLK26M_ANA_FORCE_EN         ( BIT(0) )

/* bits definitions for register REG_GLB_CLK_GEN5 */
#define BITS_CLK_SDIO_SRC_DIV(_x_)      ( (_x_) << 26 & (BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BIT_CLK_SDIO_SRC_EN             ( BIT(25) )
#define BITS_CLK_EMMCPLL_SEL(_x_)       ( (_x_) << 23 & (BIT(23)|BIT(24)) )
#define BITS_CLK_SDIO2PLL_SEL(_x_)      ( (_x_) << 21 & (BIT(21)|BIT(22)) )
#define BITS_CLK_SDIO1PLL_SEL(_x_)      ( (_x_) << 19 & (BIT(19)|BIT(20)) )
#define BITS_CLK_SDIO0PLL_SEL(_x_)      ( (_x_) << 17 & (BIT(17)|BIT(18)) )
#define BIT_LDO_USB_PD                  ( BIT(9) )
#define BITS_CLK_UART2_DIV(_x_)         ( (_x_) << 6 & (BIT(6)|BIT(7)|BIT(8)) )
#define BITS_CLK_UART1_DIV(_x_)         ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_CLK_UART0_DIV(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_GLB_DDR_PHY_RETENTION */
#define BIT_DDR_PHY_RET_STATUS          ( BIT(3) )
#define BIT_DDR_PHY_RET_CLEAR           ( BIT(2) )
#define BIT_DDR_PHY_AUTO_RET_EN         ( BIT(1) )
#define BIT_FORCE_DDR_PHY_RET           ( BIT(0) )

/* bits definitions for register REG_GLB_MM_PWR_CTL */
#define BITS_MM_PWR_ON_DLY(_x_)         ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_MM_POW_FORCE_PD             ( BIT(23) )
#define BIT_MM_SLP_POWOFF_AUTO_EN       ( BIT(22) )
#define BIT_MM_PCELL_SWAP               ( BIT(21) )
#define BITS_PD_MM_STATUS(_x_)          ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_MM_ISO_ON_NUM(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_MM_ISO_OFF_NUM(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_MM_PWR_ON_DLY              ( 24 )
#define MASK_MM_PWR_ON_DLY              ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_MM_ISO_ON_NUM              ( 8 )
#define MASK_MM_ISO_ON_NUM              ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_MM_ISO_OFF_NUM             ( 0 )
#define MASK_MM_ISO_OFF_NUM             ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_CEVA_L1RAM_PWR_CTL */
#define BITS_CEVA_L1RAM_PWR_ON_DLY(_x_) ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_CEVA_L1RAM_POW_FORCE_PD     ( BIT(23) )
#define BIT_CEVA_L1RAM_SLP_POWOFF_AUTO_EN ( BIT(22) )
#define BIT_CEVA_L1RAM_PCELL_SWAP       ( BIT(21) )
#define BITS_PD_CEVA_L1RAM_STATUS(_x_)  ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_CEVA_L1RAM_ISO_ON_NUM(_x_) ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CEVA_L1RAM_ISO_OFF_NUM(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_CEVA_L1RAM_PWR_ON_DLY      ( 24 )
#define MASK_CEVA_L1RAM_PWR_ON_DLY      ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_CEVA_L1RAM_ISO_ON_NUM      ( 8 )
#define MASK_CEVA_L1RAM_ISO_ON_NUM      ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_CEVA_L1RAM_ISO_OFF_NUM     ( 0 )
#define MASK_CEVA_L1RAM_ISO_OFF_NUM     ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_GSM_PWR_CTL */
#define BITS_GSM_PWR_ON_DLY(_x_)        ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_MCU_GSM_POW_FORCE_PD        ( BIT(23) )
#define BIT_GSM_PCELL_SWAP              ( BIT(21) )
#define BITS_PD_GSM_STATUS(_x_)         ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_GSM_ISO_ON_NUM(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_GSM_ISO_OFF_NUM(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_GSM_PWR_ON_DLY             ( 24 )
#define MASK_GSM_PWR_ON_DLY             ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_GSM_ISO_ON_NUM             ( 8 )
#define MASK_GSM_ISO_ON_NUM             ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_GSM_ISO_OFF_NUM            ( 0 )
#define MASK_GSM_ISO_OFF_NUM            ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_TD_PWR_CTL */
#define BITS_TD_PWR_ON_DLY(_x_)         ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_MCU_TD_POW_FORCE_PD         ( BIT(23) )
#define BIT_TD_PCELL_SWAP               ( BIT(21) )
#define BITS_PD_TD_STATUS(_x_)          ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_TD_ISO_ON_NUM(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_TD_ISO_OFF_NUM(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_TD_PWR_ON_DLY              ( 24 )
#define MASK_TD_PWR_ON_DLY              ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_TD_ISO_ON_NUM              ( 8 )
#define MASK_TD_ISO_ON_NUM              ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_TD_ISO_OFF_NUM             ( 0 )
#define MASK_TD_ISO_OFF_NUM             ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_PERI_PWR_CTL */
#define BITS_PERI_PWR_ON_DLY(_x_)       ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_PERI_POW_FORCE_PD           ( BIT(23) )
#define BIT_PERI_SLP_POWOFF_AUTO_EN     ( BIT(22) )
#define BIT_PERI_PCELL_SWAP             ( BIT(21) )
#define BITS_PD_PERI_STATUS(_x_)        ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_PERI_ISO_ON_NUM(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PERI_ISO_OFF_NUM(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_PERI_PWR_ON_DLY            ( 24 )
#define MASK_PERI_PWR_ON_DLY            ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_PERI_ISO_ON_NUM            ( 8 )
#define MASK_PERI_ISO_ON_NUM            ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_PERI_ISO_OFF_NUM           ( 0 )
#define MASK_PERI_ISO_OFF_NUM           ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_ARM_SYS_PWR_CTL */
#define BITS_ARM_SYS_PWR_ON_DLY(_x_)    ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_ARM_SYS_POW_FORCE_PD        ( BIT(23) )
#define BIT_ARM_SYS_SLP_POWOFF_AUTO_EN  ( BIT(22) )
#define BIT_ARM_SYS_PCELL_SWAP          ( BIT(21) )
#define BITS_PD_ARM_SYS_STATUS(_x_)     ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_ARM_SYS_ISO_ON_NUM(_x_)    ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ARM_SYS_ISO_OFF_NUM(_x_)   ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_ARM_SYS_PWR_ON_DLY         ( 24 )
#define MASK_ARM_SYS_PWR_ON_DLY         ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_ARM_SYS_ISO_ON_NUM         ( 8 )
#define MASK_ARM_SYS_ISO_ON_NUM         ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_ARM_SYS_ISO_OFF_NUM        ( 0 )
#define MASK_ARM_SYS_ISO_OFF_NUM        ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_G3D_PWR_CTL */
#define BITS_G3D_PWR_ON_DLY(_x_)        ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_G3D_POW_FORCE_PD            ( BIT(23) )
#define BIT_G3D_SLP_POWOFF_AUTO_EN      ( BIT(22) )
#define BIT_G3D_PCELL_SWAP              ( BIT(21) )
#define BITS_PD_G3D_STATUS(_x_)         ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_G3D_ISO_ON_NUM(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_G3D_ISO_OFF_NUM(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_G3D_PWR_ON_DLY             ( 24 )
#define MASK_G3D_PWR_ON_DLY             ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_G3D_ISO_ON_NUM             ( 8 )
#define MASK_G3D_ISO_ON_NUM             ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_G3D_ISO_OFF_NUM            ( 0 )
#define MASK_G3D_ISO_OFF_NUM            ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_CHIP_DSLEEP_STAT */
#define BIT_CHIP_DSLEEP                 ( BIT(0) )

/* bits definitions for register REG_GLB_PWR_CTRL_NUM1 */
#define BITS_ARM_SYS_PWR_OFF_NUM(_x_)   ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_ARM_SYS_PWR_ON_NUM(_x_)    ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_ARM_PWR_OFF_NUM(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ARM_PWR_ON_NUM(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_GLB_PWR_CTRL_NUM2 */
#define BITS_G3D_PWR_OFF_NUM(_x_)       ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_G3D_PWR_ON_NUM(_x_)        ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_MM_PWR_OFF_NUM(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_MM_PWR_ON_NUM(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_GLB_PWR_CTRL_NUM3 */
#define BITS_CEVA_L1RAM_PWR_OFF_NUM(_x_)( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_CEVA_L1RAM_PWR_ON_NUM(_x_) ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PERI_PWR_OFF_NUM(_x_)      ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PERI_PWR_ON_NUM(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_GLB_PWR_CTRL_NUM4 */
#define BITS_GSM_PWR_OFF_NUM(_x_)       ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_GSM_PWR_ON_NUM(_x_)        ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_TD_PWR_OFF_NUM(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_TD_PWR_ON_NUM(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_GLB_PWR_CTRL_NUM5 */
#define BITS_ARM9_PWR_OFF_NUM(_x_)      ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ARM9_PWR_ON_NUM(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_GLB_ARM9_SYS_PWR_CTL */
#define BITS_ARM9_SYS_PWR_ON_DLY(_x_)   ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_ARM9_SYS_POW_FORCE_PD       ( BIT(23) )
#define BIT_ARM9_SYS_SLP_POWOFF_AUTO_EN ( BIT(22) )
#define BIT_ARM9_SYS_PCELL_SWAP         ( BIT(21) )
#define BITS_PD_ARM9_SYS_STATUS(_x_)    ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BITS_ARM9_SYS_ISO_ON_NUM(_x_)   ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ARM9_SYS_ISO_OFF_NUM(_x_)  ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#define SHFT_ARM9_SYS_PWR_ON_DLY        ( 24 )
#define MASK_ARM9_SYS_PWR_ON_DLY        ( BIT(24)|BIT(25)|BIT(26) )

#define SHFT_ARM9_SYS_ISO_ON_NUM        ( 8 )
#define MASK_ARM9_SYS_ISO_ON_NUM        ( BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15) )

#define SHFT_ARM9_SYS_ISO_OFF_NUM       ( 0 )
#define MASK_ARM9_SYS_ISO_OFF_NUM       ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7) )

/* bits definitions for register REG_GLB_DMA_CTRL */
#define BIT_DMA_SPI_SEL                 ( BIT(0) )

/* bits definitions for register REG_GLB_TP_DLYC_LEN */
#define BITS_TD(_x_)                    ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register REG_GLB_MIPI_PHY_CTRL */
#define BIT_FORCE_DSI_PHY_RSTZ          ( BIT(3) )
#define BIT_FORCE_DSI_PHY_SHUTDWNZ      ( BIT(2) )
#define BIT_FORCE_CSI_PHY_RSTZ          ( BIT(1) )
#define BIT_FORCE_CSI_PHY_SHUTDWNZ      ( BIT(0) )

/* vars definitions for controller REGS_GLB */
#define REG_GLB_SET(A)                  ( A + 0x1000 )
#define REG_GLB_CLR(A)                  ( A + 0x2000 )

#endif //__REGS_GLB_H__
