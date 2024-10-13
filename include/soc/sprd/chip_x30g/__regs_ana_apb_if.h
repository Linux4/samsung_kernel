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


#ifndef __H_REGS_ANA_APB_IF_HEADFILE_H__
#define __H_REGS_ANA_APB_IF_HEADFILE_H__ __FILE__

#define	REGS_ANA_APB_IF

/* registers definitions for ANA_APB_IF */
#define REG_ANA_APB_IF_ARM_MODULE_EN                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0000)
#define REG_ANA_APB_IF_ARM_CLK_EN                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0004)
#define REG_ANA_APB_IF_RTC_CLK_EN                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0008)
#define REG_ANA_APB_IF_ARM_RST                            SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x000C)
#define REG_ANA_APB_IF_LDO_DCDC_PD_RTCSET                 SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0010)
#define REG_ANA_APB_IF_LDO_DCDC_PD_RTCCLR                 SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0014)
#define REG_ANA_APB_IF_RTC_CTRL                           SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0018)
#define REG_ANA_APB_IF_LDO_PD_CTRL                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x001C)
#define REG_ANA_APB_IF_LDO_V_CTRL0                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0020)
#define REG_ANA_APB_IF_LDO_V_CTRL1                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0024)
#define REG_ANA_APB_IF_LDO_V_CTRL2                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0028)
#define REG_ANA_APB_IF_LDO_CAL_CTRL0                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x002C)
#define REG_ANA_APB_IF_LDO_CAL_CTRL1                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0030)
#define REG_ANA_APB_IF_LDO_CAL_CTRL2                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0034)
#define REG_ANA_APB_IF_LDO_CAL_CTRL3                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0038)
#define REG_ANA_APB_IF_LDO_CAL_CTRL4                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x003C)
#define REG_ANA_APB_IF_LDO_CAL_CTRL5                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0040)
#define REG_ANA_APB_IF_LDO_CAL_CTRL6                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0044)
#define REG_ANA_APB_IF_AUXAD_CTL                          SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0048)
#define REG_ANA_APB_IF_DCDC_CTRL0                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x004C)
#define REG_ANA_APB_IF_DCDC_CTRL1                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0050)
#define REG_ANA_APB_IF_DCDC_CTRL2                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0054)
#define REG_ANA_APB_IF_DCDC_CTRL3                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0058)
#define REG_ANA_APB_IF_DCDC_CTRL4                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x005C)
#define REG_ANA_APB_IF_DCDC_CTRL5                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0060)
#define REG_ANA_APB_IF_DCDC_CTRL6                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0064)
#define REG_ANA_APB_IF_DDR2_CTRL                          SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0068)
#define REG_ANA_APB_IF_SLP_WAIT_DCDCARM                   SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x006C)
#define REG_ANA_APB_IF_LDO1828_XTL_CTL                    SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0070)
#define REG_ANA_APB_IF_LDO_SLP_CTRL0                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0074)
#define REG_ANA_APB_IF_LDO_SLP_CTRL1                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0078)
#define REG_ANA_APB_IF_LDO_SLP_CTRL2                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x007C)
#define REG_ANA_APB_IF_LDO_SLP_CTRL3                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0080)
#define REG_ANA_APB_IF_AUD_SLP_CTRL4                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0084)
#define REG_ANA_APB_IF_DCDC_SLP_CTRL                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0088)
#define REG_ANA_APB_IF_XTL_WAIT_CTRL                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x008C)
#define REG_ANA_APB_IF_FLASH_CTRL                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0090)
#define REG_ANA_APB_IF_WHTLED_CTRL0                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0094)
#define REG_ANA_APB_IF_WHTLED_CTRL1                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0098)
#define REG_ANA_APB_IF_WHTLED_CTRL2                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x009C)
#define REG_ANA_APB_IF_ANA_DRV_CTRL                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00A0)
#define REG_ANA_APB_IF_VIBR_CTRL0                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00A4)
#define REG_ANA_APB_IF_VIBR_CTRL1                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00A8)
#define REG_ANA_APB_IF_VIBR_CTRL2                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00AC)
#define REG_ANA_APB_IF_VIBR_WR_PROT_VALUE                 SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00B0)
#define REG_ANA_APB_IF_AUDIO_CTRL                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00B4)
#define REG_ANA_APB_IF_CHGR_CTRL0                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00B8)
#define REG_ANA_APB_IF_CHGR_CTRL1                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00BC)
#define REG_ANA_APB_IF_CHGR_CTRL2                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00C0)
#define REG_ANA_APB_IF_CHGR_STATUS                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00C4)
#define REG_ANA_APB_IF_ANA_MIXED_CTRL                     SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00C8)
#define REG_ANA_APB_IF_PWR_XTL_EN0                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00CC)
#define REG_ANA_APB_IF_PWR_XTL_EN1                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00D0)
#define REG_ANA_APB_IF_PWR_XTL_EN2                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00D4)
#define REG_ANA_APB_IF_PWR_XTL_EN3                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00D8)
#define REG_ANA_APB_IF_PWR_XTL_EN4                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00DC)
#define REG_ANA_APB_IF_PWR_XTL_EN5                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00E0)
#define REG_ANA_APB_IF_ANA_STATUS                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00E4)
#define REG_ANA_APB_IF_POR_RST_MONITOR                    SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00E8)
#define REG_ANA_APB_IF_WDG_RST_MONITOR                    SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00EC)
#define REG_ANA_APB_IF_POR_PIN_RST_MONITOR                SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00F0)
#define REG_ANA_APB_IF_POR_SRC_FLAG                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00F4)
#define REG_ANA_APB_IF_POR_7S_CTRL                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00F8)
#define REG_ANA_APB_IF_INT_DEBUG                          SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x00FC)
#define REG_ANA_APB_IF_GPI_DEBUG                          SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0100)
#define REG_ANA_APB_IF_HWRST_RTC                          SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0104)
#define REG_ANA_APB_IF_CHIP_ID_LOW                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0108)
#define REG_ANA_APB_IF_CHIP_ID_HIGH                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x010C)
#define REG_ANA_APB_IF_ARM_MF_REG                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0110)
#define REG_ANA_APB_IF_AFUSE_CTRL                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0114)
#define REG_ANA_APB_IF_AFUSE_OUT0                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0118)
#define REG_ANA_APB_IF_AFUSE_OUT1                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x011C)
#define REG_ANA_APB_IF_AFUSE_OUT2                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0120)
#define REG_ANA_APB_IF_AFUSE_OUT3                         SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0124)
#define REG_ANA_APB_IF_CA_CTRL4                           SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0128)
#define REG_ANA_APB_IF_MCU_WR_PROT_VALUE                  SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x012C)
#define REG_ANA_APB_IF_MP_PWR_CTRL0                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0130 )
#define REG_ANA_APB_IF_MP_PWR_CTRL1                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0134)
#define REG_ANA_APB_IF_MP_PWR_CTRL2                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0138)
#define REG_ANA_APB_IF_MP_PWR_CTRL3                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x013C)
#define REG_ANA_APB_IF_MP_LDO_CTRL                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0140)
#define REG_ANA_APB_IF_MP_CHG_CTRL                        SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0144)
#define REG_ANA_APB_IF_MP_MISC_CTRL                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0148)
#define REG_ANA_APB_IF_MP_DCDC_DEDT_CTRL                  SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x014C)
#define REG_ANA_APB_IF_CA_CTRL0                           SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0150)
#define REG_ANA_APB_IF_CA_CTRL1                           SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0154)
#define REG_ANA_APB_IF_CA_CTRL2                           SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0158)
#define REG_ANA_APB_IF_CA_CTRL3                           SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x015C)
#define REG_ANA_APB_IF_DCDC_CORE_ADI                      SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0160)
#define REG_ANA_APB_IF_DCDC_ARM_ADI                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0164)
#define REG_ANA_APB_IF_DCDC_MEM_ADI                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0168)
#define REG_ANA_APB_IF_DCDC_GEN_ADI                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x016C)
#define REG_ANA_APB_IF_DCDC_WRF_ADI                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0170)
#define REG_ANA_APB_IF_DCDC_WPA_ADI                       SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x0174)
#define REG_ANA_APB_IF_DCDC_WPA_DCM_ADI                   SCI_ADDR(REGS_ANA_APB_IF_BASE, 0x017C)



/* bits definitions for register REG_ANA_APB_IF_ARM_MODULE_EN */
#define BIT_ANA_THM_EN                                    ( BIT(12) )
#define BIT_ANA_BLTC_EN                                   ( BIT(10) )
#define BIT_ANA_PINREG_EN                                 ( BIT(9) )
#define BIT_ANA_FGU_EN                                    ( BIT(8) )
#define BIT_ANA_GPIO_EN                                   ( BIT(7) )
#define BIT_ANA_ADC_EN                                    ( BIT(6) )
#define BIT_ANA_HDT_EN                                    ( BIT(5) )
#define BIT_ANA_AUD_EN                                    ( BIT(4) )
#define BIT_ANA_EIC_EN                                    ( BIT(3) )
#define BIT_ANA_WDG_EN                                    ( BIT(2) )
#define BIT_ANA_RTC_EN                                    ( BIT(1) )

/* bits definitions for register REG_ANA_APB_IF_ARM_CLK_EN */
#define BIT_ANA_CLK_AUXAD_EN                              ( BIT(9) )
#define BIT_ANA_CLK_AUXADC_EN                             ( BIT(8) )
#define BIT_ANA_CLK_AUD_HID_EN                            ( BIT(5) )
#define BIT_ANA_CLK_AUD_HBD_EN                            ( BIT(4) )
#define BIT_ANA_CLK_AUD_LOOP_EN                           ( BIT(3) )
#define BIT_ANA_CLK_AUD_6P5M_EN                           ( BIT(2) )
#define BIT_ANA_CLK_AUDIF_EN                              ( BIT(1) )

/* bits definitions for register REG_ANA_APB_IF_RTC_CLK_EN */
#define BIT_ANA_RTC_FLASH_EN                              ( BIT(13) )
#define BIT_ANA_RTC_THMA_AUTO_EN                          ( BIT(12) )
#define BIT_ANA_RTC_THMA_EN                               ( BIT(11) )
#define BIT_ANA_RTC_THM_EN                                ( BIT(10) )
#define BIT_ANA_RTC_BLTC_EN                               ( BIT(8) )
#define BIT_ANA_RTC_FGU_EN                                ( BIT(7) )
#define BIT_ANA_RTC_FGUA_EN                               ( BIT(6) )
#define BIT_ANA_RTC_VIBR_EN                               ( BIT(5) )
#define BIT_ANA_RTC_AUD_EN                                ( BIT(4) )
#define BIT_ANA_RTC_EIC_EN                                ( BIT(3) )
#define BIT_ANA_RTC_WDG_EN                                ( BIT(2) )
#define BIT_ANA_RTC_RTC_EN                                ( BIT(1) )
#define BIT_ANA_RTC_ARCH_EN                               ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_ARM_RST */
#define BIT_ANA_THMA_SOFT_RST                             ( BIT(14) )
#define BIT_ANA_THM_SOFT_RST                              ( BIT(13) )
#define BIT_ANA_AUD_32K_SOFT_RST                          ( BIT(12) )
#define BIT_ANA_AUDTX_SOFT_RST                            ( BIT(11) )
#define BIT_ANA_AUDRX_SOFT_RST                            ( BIT(10) )
#define BIT_ANA_AUD_SOFT_RST                              ( BIT(9) )
#define BIT_ANA_AUD_HDT_SOFT_RST                          ( BIT(8) )
#define BIT_ANA_GPIO_SOFT_RST                             ( BIT(7) )
#define BIT_ANA_ADC_SOFT_RST                              ( BIT(6) )
#define BIT_ANA_PWM0_SOFT_RST                             ( BIT(5) )
#define BIT_ANA_FGU_SOFT_RST                              ( BIT(4) )
#define BIT_ANA_EIC_SOFT_RST                              ( BIT(3) )
#define BIT_ANA_WDG_SOFT_RST                              ( BIT(2) )
#define BIT_ANA_RTC_SOFT_RST                              ( BIT(1) )
#define BIT_ANA_BLTC_SOFT_RST                             ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_LDO_DCDC_PD_RTCSET */
#define BIT_ANA_LDO_AVDD18_PD_RTCSET                      ( BIT(15) )
#define BIT_ANA_DCDC_OTP_PD_RTCSET                        ( BIT(14) )
#define BIT_ANA_DCDC_WRF_PD_RTCSET                        ( BIT(13) )
#define BIT_ANA_DCDC_GEN_PD_RTCSET                        ( BIT(12) )
#define BIT_ANA_DCDC_MEM_PD_RTCSET                        ( BIT(11) )
#define BIT_ANA_DCDC_ARM_PD_RTCSET                        ( BIT(10) )
#define BIT_ANA_DCDC_CORE_PD_RTCSET                       ( BIT(9) )
#define BIT_ANA_LDO_EMMCCORE_PD_RTCSET                    ( BIT(8) )
#define BIT_ANA_LDO_EMMCIO_PD_RTCSET                      ( BIT(7) )
#define BIT_ANA_LDO_RF2_PD_RTCSET                         ( BIT(6) )
#define BIT_ANA_LDO_RF1_PD_RTCSET                         ( BIT(5) )
#define BIT_ANA_LDO_RF0_PD_RTCSET                         ( BIT(4) )
#define BIT_ANA_LDO_VDD25_PD_RTCSET                       ( BIT(3) )
#define BIT_ANA_LDO_VDD28_PD_RTCSET                       ( BIT(2) )
#define BIT_ANA_LDO_VDD18_PD_RTCSET                       ( BIT(1) )
#define BIT_ANA_BG_PD_RTCSET                              ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_LDO_DCDC_PD_RTCCLR */
#define BIT_ANA_LDO_AVDD18_PD_RTCCLR                      ( BIT(15) )
#define BIT_ANA_DCDC_OTP_PD_RTCCLR                        ( BIT(14) )
#define BIT_ANA_DCDC_WRF_PD_RTCCLR                        ( BIT(13) )
#define BIT_ANA_DCDC_GEN_PD_RTCCLR                        ( BIT(12) )
#define BIT_ANA_DCDC_MEM_PD_RTCCLR                        ( BIT(11) )
#define BIT_ANA_DCDC_ARM_PD_RTCCLR                        ( BIT(10) )
#define BIT_ANA_DCDC_CORE_PD_RTCCLR                       ( BIT(9) )
#define BIT_ANA_LDO_EMMCCORE_PD_RTCCLR                    ( BIT(8) )
#define BIT_ANA_LDO_EMMCIO_PD_RTCCLR                      ( BIT(7) )
#define BIT_ANA_LDO_RF2_PD_RTCCLR                         ( BIT(6) )
#define BIT_ANA_LDO_RF1_PD_RTCCLR                         ( BIT(5) )
#define BIT_ANA_LDO_RF0_PD_RTCCLR                         ( BIT(4) )
#define BIT_ANA_LDO_VDD25_PD_RTCCLR                       ( BIT(3) )
#define BIT_ANA_LDO_VDD28_PD_RTCCLR                       ( BIT(2) )
#define BIT_ANA_LDO_VDD18_PD_RTCCLR                       ( BIT(1) )
#define BIT_ANA_BG_PD_RTCCLR                              ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_RTC_CTRL */
#define BITS_ANA_XOSC32K_CTL_STS(_X_)                     ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_ANA_XOSC32K_CTL_B3_RTCCLR                     ( BIT(7) )
#define BIT_ANA_XOSC32K_CTL_B2_RTCCLR                     ( BIT(6) )
#define BIT_ANA_XOSC32K_CTL_B1_RTCCLR                     ( BIT(5) )
#define BIT_ANA_XOSC32K_CTL_B0_RTCCLR                     ( BIT(4) )
#define BIT_ANA_XOSC32K_CTL_B3_RTCSET                     ( BIT(3) )
#define BIT_ANA_XOSC32K_CTL_B2_RTCSET                     ( BIT(2) )
#define BIT_ANA_XOSC32K_CTL_B1_RTCSET                     ( BIT(1) )
#define BIT_ANA_XOSC32K_CTL_B0_RTCSET                     ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_LDO_PD_CTRL */
#define BIT_ANA_LDO_LPREF_PD_SW                           ( BIT(12) )
#define BIT_ANA_DCDC_WPA_PD                               ( BIT(11) )
#define BIT_ANA_LDO_CLSG_PD                               ( BIT(10) )
#define BIT_ANA_LDO_USB_PD                                ( BIT(9) )
#define BIT_ANA_LDO_CAMMOT_PD                             ( BIT(8) )
#define BIT_ANA_LDO_CAMIO_PD                              ( BIT(7) )
#define BIT_ANA_LDO_CAMD_PD                               ( BIT(6) )
#define BIT_ANA_LDO_CAMA_PD                               ( BIT(5) )
#define BIT_ANA_LDO_SIM2_PD                               ( BIT(4) )
#define BIT_ANA_LDO_SIM1_PD                               ( BIT(3) )
#define BIT_ANA_LDO_SIM0_PD                               ( BIT(2) )
#define BIT_ANA_LDO_SD_PD                                 ( BIT(1) )

/* bits definitions for register REG_ANA_APB_IF_LDO_V_CTRL0 */
#define BITS_ANA_LDO_EMMCCORE_V(_X_)                      ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_ANA_LDO_EMMCIO_V(_X_)                        ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_ANA_LDO_RF2_V(_X_)                           ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_ANA_LDO_RF1_V(_X_)                           ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ANA_LDO_RF0_V(_X_)                           ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_ANA_LDO_VDD25_V(_X_)                         ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_ANA_LDO_VDD28_V(_X_)                         ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_ANA_LDO_VDD18_V(_X_)                         ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ANA_APB_IF_LDO_V_CTRL1 */
#define BITS_ANA_LDO_SIM2_V(_X_)                          ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ANA_LDO_SIM1_V(_X_)                          ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_ANA_LDO_SIM0_V(_X_)                          ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_ANA_LDO_SD_V(_X_)                            ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_ANA_LDO_AVDD18_V(_X_)                        ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ANA_APB_IF_LDO_V_CTRL2 */
#define BITS_ANA_VBATBK_RES(_X_)                          ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_ANA_VBATBK_V(_X_)                            ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_ANA_LDO_CLSG_V(_X_)                          ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_ANA_LDO_USB_V(_X_)                           ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ANA_LDO_CAMMOT_V(_X_)                        ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_ANA_LDO_CAMIO_V(_X_)                         ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_ANA_LDO_CAMD_V(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_ANA_LDO_CAMA_V(_X_)                          ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ANA_APB_IF_LDO_CAL_CTRL0 */
#define BITS_ANA_LDO_VDD25_CAL(_X_)                       ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_ANA_LDO_VDD28_CAL(_X_)                       ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_LDO_VDD18_CAL(_X_)                       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_LDO_CAL_CTRL1 */
#define BITS_ANA_LDO_RF2_CAL(_X_)                         ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_ANA_LDO_RF1_CAL(_X_)                         ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_LDO_RF0_CAL(_X_)                         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_LDO_CAL_CTRL2 */
#define BITS_ANA_LDO_USB_CAL(_X_)                         ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_ANA_LDO_EMMCCORE_CAL(_X_)                    ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_LDO_EMMCIO_CAL(_X_)                      ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_LDO_CAL_CTRL3 */
#define BITS_ANA_LDO_SD_CAL(_X_)                          ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_LDO_AVDD18_CAL(_X_)                      ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_LDO_CAL_CTRL4 */
#define BITS_ANA_LDO_CAMA_CAL(_X_)                        ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_ANA_LDO_SIM2_CAL(_X_)                        ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_LDO_SIM_CAL(_X_)                         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_LDO_CAL_CTRL5 */
#define BITS_ANA_LDO_CAMMOT_CAL(_X_)                      ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_ANA_LDO_CAMIO_CAL(_X_)                       ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_LDO_CAMD_CAL(_X_)                        ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_LDO_CAL_CTRL6 */
#define BITS_ANA_LDOA_CAL_SEL(_X_)                        ( (_X_) << 11 & (BIT(11)|BIT(12)|BIT(13)) )
#define BITS_ANA_LDOD_CAL_SEL(_X_)                        ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_ANA_LDODCDC_CAL_SEL(_X_)                     ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)) )
#define BITS_ANA_LDO_CLSG_CAL(_X_)                        ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_AUXAD_CTL */
#define BIT_ANA_AUXAD_CURRENTSEN_EN                       ( BIT(6) )
#define BIT_ANA_AUXAD_CURRENTSEL                          ( BIT(5) )
#define BITS_ANA_AUXAD_CURRENT_IBS(_X_)                   ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_CTRL0 */
#define BIT_ANA_DCDC_CORE_LP_EN                           ( BIT(15) )
#define BIT_ANA_DCDC_CORE_CL_CTRL                         ( BIT(14) )
#define BITS_ANA_DCDC_CORE_PDRSLOW(_X_)                   ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_ANA_DCDC_CORE_PFM                             ( BIT(9) )
#define BIT_ANA_DCDC_CORE_DCM                             ( BIT(8) )
#define BIT_ANA_DCDC_ARM_LP_EN                            ( BIT(7) )
#define BIT_ANA_DCDC_ARM_CL_CTRL                          ( BIT(6) )
#define BITS_ANA_DCDC_ARM_PDRSLOW(_X_)                    ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)) )
#define BIT_ANA_DCDC_ARM_PFM                              ( BIT(1) )
#define BIT_ANA_DCDC_ARM_DCM                              ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_CTRL1 */
#define BIT_ANA_DCDC_MEM_LP_EN                            ( BIT(15) )
#define BIT_ANA_DCDC_MEM_CL_CTRL                          ( BIT(14) )
#define BITS_ANA_DCDC_MEM_PDRSLOW(_X_)                    ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_ANA_DCDC_MEM_PFM                              ( BIT(9) )
#define BIT_ANA_DCDC_MEM_DCM                              ( BIT(8) )
#define BIT_ANA_DCDC_GEN_LP_EN                            ( BIT(7) )
#define BIT_ANA_DCDC_GEN_CL_CTRL                          ( BIT(6) )
#define BITS_ANA_DCDC_GEN_PDRSLOW(_X_)                    ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)) )
#define BIT_ANA_DCDC_GEN_PFM                              ( BIT(1) )
#define BIT_ANA_DCDC_GEN_DCM                              ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_CTRL2 */
#define BIT_ANA_DCDC_BG_LP_EN                             ( BIT(15) )
#define BIT_ANA_DCDC_OTP_CHIP_PD_FLAG_CLR                 ( BIT(14) )
#define BIT_ANA_DCDC_OTP_CHIP_PD_FLAG                     ( BIT(13) )
#define BIT_ANA_DCDC_OTP_AUTO_PD_EN                       ( BIT(12) )
#define BIT_ANA_DCDC_OTP_VBEOP                            ( BIT(11) )
#define BITS_ANA_DCDC_OTP_S(_X_)                          ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BIT_ANA_DCDC_WRF_LP_EN                            ( BIT(7) )
#define BIT_ANA_DCDC_WRF_CL_CTRL                          ( BIT(6) )
#define BITS_ANA_DCDC_WRF_PDRSLOW(_X_)                    ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)) )
#define BIT_ANA_DCDC_WRF_PFM                              ( BIT(1) )
#define BIT_ANA_DCDC_WRF_DCM                              ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_CTRL3 */
#define BIT_ANA_DCDC_WPA_LP_EN                            ( BIT(8) )
#define BIT_ANA_DCDC_WPA_CL_CTRL                          ( BIT(7) )
#define BIT_ANA_DCDC_WPA_ZXOP                             ( BIT(6) )
#define BITS_ANA_DCDC_WPA_PDRSLOW(_X_)                    ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)) )
#define BIT_ANA_DCDC_WPA_PFM                              ( BIT(1) )
#define BIT_ANA_DCDC_WPA_APTEN                            ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_CTRL4 */
#define BIT_ANA_DCDC_CORE_OSCSYCEN_SW                     ( BIT(15) )
#define BIT_ANA_DCDC_CORE_OSCSYCEN_HW_EN                  ( BIT(14) )
#define BIT_ANA_DCDC_CORE_OSCSYC_DIV_EN                   ( BIT(13) )
#define BITS_ANA_DCDC_CORE_OSCSYC_DIV(_X_)                ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BIT_ANA_DCDC_ARM_OSCSYCEN_SW                      ( BIT(7) )
#define BIT_ANA_DCDC_ARM_OSCSYCEN_HW_EN                   ( BIT(6) )
#define BIT_ANA_DCDC_ARM_OSCSYC_DIV_EN                    ( BIT(5) )
#define BITS_ANA_DCDC_ARM_OSCSYC_DIV(_X_)                 ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_CTRL5 */
#define BIT_ANA_DCDC_MEM_OSCSYCEN_SW                      ( BIT(15) )
#define BIT_ANA_DCDC_MEM_OSCSYCEN_HW_EN                   ( BIT(14) )
#define BIT_ANA_DCDC_MEM_OSCSYC_DIV_EN                    ( BIT(13) )
#define BITS_ANA_DCDC_MEM_OSCSYC_DIV(_X_)                 ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BIT_ANA_DCDC_GEN_OSCSYCEN_SW                      ( BIT(7) )
#define BIT_ANA_DCDC_GEN_OSCSYCEN_HW_EN                   ( BIT(6) )
#define BIT_ANA_DCDC_GEN_OSCSYC_DIV_EN                    ( BIT(5) )
#define BITS_ANA_DCDC_GEN_OSCSYC_DIV(_X_)                 ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_CTRL6 */
#define BIT_ANA_DCDC_WPA_OSCSYCEN_SW                      ( BIT(15) )
#define BIT_ANA_DCDC_WPA_OSCSYCEN_HW_EN                   ( BIT(14) )
#define BIT_ANA_DCDC_WPA_OSCSYC_DIV_EN                    ( BIT(13) )
#define BITS_ANA_DCDC_WPA_OSCSYC_DIV(_X_)                 ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BIT_ANA_DCDC_WRF_OSCSYCEN_SW                      ( BIT(7) )
#define BIT_ANA_DCDC_WRF_OSCSYCEN_HW_EN                   ( BIT(6) )
#define BIT_ANA_DCDC_WRF_OSCSYC_DIV_EN                    ( BIT(5) )
#define BITS_ANA_DCDC_WRF_OSCSYC_DIV(_X_)                 ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_DDR2_CTRL */
#define BIT_ANA_DDR2_BUF_PD_HW                            ( BIT(9) )
#define BITS_ANA_DDR2_BUF_S_DS(_X_)                       ( (_X_) << 7 & (BIT(7)|BIT(8)) )
#define BITS_ANA_DDR2_BUF_CHNS_DS(_X_)                    ( (_X_) << 5 & (BIT(5)|BIT(6)) )
#define BIT_ANA_DDR2_BUF_PD                               ( BIT(4) )
#define BITS_ANA_DDR2_BUF_S(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_ANA_DDR2_BUF_CHNS(_X_)                       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ANA_APB_IF_SLP_WAIT_DCDCARM */
#define BITS_ANA_SLP_IN_WAIT_DCDCARM(_X_)                 ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ANA_SLP_OUT_WAIT_DCDCARM(_X_)                ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_ANA_APB_IF_LDO1828_XTL_CTL */
#define BIT_ANA_LDO_VDD18_EXT_XTL2_EN                     ( BIT(11) )
#define BIT_ANA_LDO_VDD18_EXT_XTL1_EN                     ( BIT(10) )
#define BIT_ANA_LDO_VDD18_EXT_XTL0_EN                     ( BIT(9) )
#define BIT_ANA_LDO_VDD18_XTL2_EN                         ( BIT(8) )
#define BIT_ANA_LDO_VDD18_XTL1_EN                         ( BIT(7) )
#define BIT_ANA_LDO_VDD18_XTL0_EN                         ( BIT(6) )
#define BIT_ANA_LDO_VDD28_EXT_XTL2_EN                     ( BIT(5) )
#define BIT_ANA_LDO_VDD28_EXT_XTL1_EN                     ( BIT(4) )
#define BIT_ANA_LDO_VDD28_EXT_XTL0_EN                     ( BIT(3) )
#define BIT_ANA_LDO_VDD28_XTL2_EN                         ( BIT(2) )
#define BIT_ANA_LDO_VDD28_XTL1_EN                         ( BIT(1) )
#define BIT_ANA_LDO_VDD28_XTL0_EN                         ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_LDO_SLP_CTRL0 */
#define BIT_ANA_SLP_IO_EN                                 ( BIT(15) )
#define BIT_ANA_SLP_DCDC_OTP_PD_EN                        ( BIT(13) )
#define BIT_ANA_SLP_DCDCGEN_PD_EN                         ( BIT(12) )
#define BIT_ANA_SLP_DCDCWPA_PD_EN                         ( BIT(11) )
#define BIT_ANA_SLP_DCDCWRF_PD_EN                         ( BIT(10) )
#define BIT_ANA_SLP_DCDCARM_PD_EN                         ( BIT(9) )
#define BIT_ANA_SLP_LDOEMMCCORE_PD_EN                     ( BIT(7) )
#define BIT_ANA_SLP_LDOEMMCIO_PD_EN                       ( BIT(6) )
#define BIT_ANA_SLP_LDORF2_PD_EN                          ( BIT(5) )
#define BIT_ANA_SLP_LDORF1_PD_EN                          ( BIT(4) )
#define BIT_ANA_SLP_LDORF0_PD_EN                          ( BIT(3) )
#define BIT_ANA_SLP_LDOVDD25_PD_EN                        ( BIT(2) )
#define BIT_ANA_SLP_LDOVDD28_PD_EN                        ( BIT(1) )
#define BIT_ANA_SLP_LDOVDD18_PD_EN                        ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_LDO_SLP_CTRL1 */
#define BIT_ANA_SLP_LDO_PD_EN                             ( BIT(12) )
#define BIT_ANA_SLP_LDOLPREF_PD_EN                        ( BIT(11) )
#define BIT_ANA_SLP_LDOCLSG_PD_EN                         ( BIT(10) )
#define BIT_ANA_SLP_LDOUSB_PD_EN                          ( BIT(9) )
#define BIT_ANA_SLP_LDOCAMMOT_PD_EN                       ( BIT(8) )
#define BIT_ANA_SLP_LDOCAMIO_PD_EN                        ( BIT(7) )
#define BIT_ANA_SLP_LDOCAMD_PD_EN                         ( BIT(6) )
#define BIT_ANA_SLP_LDOCAMA_PD_EN                         ( BIT(5) )
#define BIT_ANA_SLP_LDOSIM2_PD_EN                         ( BIT(4) )
#define BIT_ANA_SLP_LDOSIM1_PD_EN                         ( BIT(3) )
#define BIT_ANA_SLP_LDOSIM0_PD_EN                         ( BIT(2) )
#define BIT_ANA_SLP_LDOSD_PD_EN                           ( BIT(1) )
#define BIT_ANA_SLP_LDOAVDD18_PD_EN                       ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_LDO_SLP_CTRL2 */
#define BIT_ANA_SLP_DCDC_BG_LP_EN                         ( BIT(15) )
#define BIT_ANA_SLP_DCDCCORE_LP_EN                        ( BIT(11) )
#define BIT_ANA_SLP_DCDCMEM_LP_EN                         ( BIT(10) )
#define BIT_ANA_SLP_DCDCARM_LP_EN                         ( BIT(9) )
#define BIT_ANA_SLP_DCDCGEN_LP_EN                         ( BIT(8) )
#define BIT_ANA_SLP_DCDCWPA_LP_EN                         ( BIT(6) )
#define BIT_ANA_SLP_DCDCWRF_LP_EN                         ( BIT(5) )
#define BIT_ANA_SLP_LDOEMMCCORE_LP_EN                     ( BIT(4) )
#define BIT_ANA_SLP_LDOEMMCIO_LP_EN                       ( BIT(3) )
#define BIT_ANA_SLP_LDORF2_LP_EN                          ( BIT(2) )
#define BIT_ANA_SLP_LDORF1_LP_EN                          ( BIT(1) )
#define BIT_ANA_SLP_LDORF0_LP_EN                          ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_LDO_SLP_CTRL3 */
#define BIT_ANA_SLP_BG_LP_EN                              ( BIT(15) )
#define BIT_ANA_SLP_LDOVDD25_LP_EN                        ( BIT(13) )
#define BIT_ANA_SLP_LDOVDD28_LP_EN                        ( BIT(12) )
#define BIT_ANA_SLP_LDOVDD18_LP_EN                        ( BIT(11) )
#define BIT_ANA_SLP_LDOCLSG_LP_EN                         ( BIT(10) )
#define BIT_ANA_SLP_LDOUSB_LP_EN                          ( BIT(9) )
#define BIT_ANA_SLP_LDOCAMMOT_LP_EN                       ( BIT(8) )
#define BIT_ANA_SLP_LDOCAMIO_LP_EN                        ( BIT(7) )
#define BIT_ANA_SLP_LDOCAMD_LP_EN                         ( BIT(6) )
#define BIT_ANA_SLP_LDOCAMA_LP_EN                         ( BIT(5) )
#define BIT_ANA_SLP_LDOSIM2_LP_EN                         ( BIT(4) )
#define BIT_ANA_SLP_LDOSIM1_LP_EN                         ( BIT(3) )
#define BIT_ANA_SLP_LDOSIM0_LP_EN                         ( BIT(2) )
#define BIT_ANA_SLP_LDOSD_LP_EN                           ( BIT(1) )
#define BIT_ANA_SLP_LDOAVDD18_LP_EN                       ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_AUD_SLP_CTRL4 */
#define BIT_ANA_SLP_AUD_PA_SW_PD_EN                       ( BIT(15) )
#define BIT_ANA_SLP_AUD_PA_LDO_PD_EN                      ( BIT(14) )
#define BIT_ANA_SLP_AUD_PA_PD_EN                          ( BIT(13) )
#define BIT_ANA_SLP_AUD_OVP_PD_PD_EN                      ( BIT(9) )
#define BIT_ANA_SLP_AUD_OVP_LDO_PD_EN                     ( BIT(8) )
#define BIT_ANA_SLP_AUD_VB_PD_EN                          ( BIT(7) )
#define BIT_ANA_SLP_AUD_VBO_PD_EN                         ( BIT(6) )
#define BIT_ANA_SLP_AUD_HEADMICBIAS_PD_EN                 ( BIT(5) )
#define BIT_ANA_SLP_AUD_MICBIAS_HV_PD_EN                  ( BIT(2) )
#define BIT_ANA_SLP_AUD_HEADMIC_PD_EN                     ( BIT(1) )
#define BIT_ANA_SLP_AUD_PMUR1_PD_EN                       ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_SLP_CTRL */
#define BITS_ANA_SLP_DCDCCORE_VOL_DROP_CNT(_X_)           ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_ANA_DCDC_CORE_CTL_DS_SW(_X_)                 ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_ANA_APB_IF_XTL_WAIT_CTRL */
#define BIT_ANA_SLP_XTLBUF_PD_EN                          ( BIT(9) )
#define BIT_ANA_XTL_EN                                    ( BIT(8) )
#define BITS_ANA_XTL_WAIT(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_ANA_APB_IF_FLASH_CTRL */
#define BIT_ANA_FLASH_PON                                 ( BIT(15) )
#define BIT_ANA_FLASH_V_HW_EN                             ( BIT(6) )
#define BITS_ANA_FLASH_V_HW_STEP(_X_)                     ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_ANA_FLASH_V_SW(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ANA_APB_IF_WHTLED_CTRL0 */
#define BIT_ANA_WHTLED_PD_STS                             ( BIT(15) )
#define BITS_ANA_WHTLED_DC(_X_)                           ( (_X_) << 9 & (BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BIT_ANA_WHTLED_BOOST_EN                           ( BIT(8) )
#define BIT_ANA_WHTLED_SERIES_EN                          ( BIT(7) )
#define BITS_ANA_WHTLED_V(_X_)                            ( (_X_) << 1 & (BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )
#define BIT_ANA_WHTLED_PD                                 ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_WHTLED_CTRL1 */
#define BIT_ANA_LCM_CABC_PWM_SEL                          ( BIT(15) )
#define BIT_ANA_RTC_PWM0_EN                               ( BIT(14) )
#define BIT_ANA_PWM0_EN                                   ( BIT(13) )
#define BITS_ANA_WHTLED_ISET(_X_)                         ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_ANA_WHTLED_FRE_AD(_X_)                       ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)) )
#define BITS_ANA_WHTLED_CLMIT_OP(_X_)                     ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ANA_APB_IF_WHTLED_CTRL2 */
#define BIT_ANA_WHTLED_DIMMING_SEL                        ( BIT(15) )
#define BIT_ANA_WHTLED_PD_SEL                             ( BIT(14) )
#define BIT_ANA_WHTLED_DIS_OVST                           ( BIT(13) )
#define BIT_ANA_WHTLED_DIM_SEL                            ( BIT(12) )
#define BIT_ANA_WHTLED_DE_BIAS                            ( BIT(11) )
#define BIT_ANA_WHTLED_BUFF_SHT                           ( BIT(10) )
#define BITS_ANA_WHTLED_STB_OP(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_ANA_WHTLED_CAP_OPTION                         ( BIT(7) )
#define BIT_ANA_WHTLED_OVP_DIS                            ( BIT(6) )
#define BITS_ANA_WHTLED_REF_DC(_X_)                       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_ANA_APB_IF_ANA_DRV_CTRL */
#define BIT_ANA_SLP_RGB_PD_EN                             ( BIT(14) )
#define BIT_ANA_RGB_PD_HW_EN                              ( BIT(13) )
#define BITS_ANA_RGB_V(_X_)                               ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_ANA_KPLED_V(_X_)                             ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ANA_RGB_PD_SW                                 ( BIT(2) )
#define BIT_ANA_KPLED_PD                                  ( BIT(1) )
#define BIT_ANA_KPLED_PD_STS                              ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_VIBR_CTRL0 */
#define BITS_ANA_VIBR_STABLE_V_HW(_X_)                    ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ANA_VIBR_INIT_V_HW(_X_)                      ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_ANA_VIBR_V_SW(_X_)                           ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ANA_VIBR_PON                                  ( BIT(2) )
#define BIT_ANA_VIBR_SW_EN                                ( BIT(1) )

/* bits definitions for register REG_ANA_APB_IF_VIBR_CTRL1 */
#define BITS_ANA_VIBR_V_CONVERT_CNT_HW(_X_)               ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_VIBR_CTRL2 */
#define BIT_ANA_VIBR_PWR_ON_STS                           ( BIT(15) )
#define BIT_ANA_VIBR_HW_FLOW_ERR1                         ( BIT(14) )
#define BIT_ANA_VIBR_HW_FLOW_ERR1_CLR                     ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_VIBR_WR_PROT_VALUE */
#define BIT_ANA_VIBR_WR_PROT                              ( BIT(15) )
#define BITS_ANA_VIBR_WR_PROT_VALUE(_X_)                  ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )

/* bits definitions for register REG_ANA_APB_IF_AUDIO_CTRL */
#define BIT_ANA_AUD_SLP_APP_RST_EN                        ( BIT(10) )
#define BITS_ANA_CLK_AUD_HBD_DIV(_X_)                     ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BIT_ANA_CLK_AUD_LOOP_INV_EN                       ( BIT(4) )
#define BIT_ANA_CLK_AUDIF_TX_INV_EN                       ( BIT(3) )
#define BIT_ANA_CLK_AUDIF_RX_INV_EN                       ( BIT(2) )
#define BIT_ANA_CLK_AUD_6P5M_TX_INV_EN                    ( BIT(1) )
#define BIT_ANA_CLK_AUD_6P5M_RX_INV_EN                    ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_CHGR_CTRL0 */
#define BIT_ANA_CHGR_PD_STS                               ( BIT(15) )
#define BITS_ANA_CHGR_CV_V(_X_)                           ( (_X_) << 6 & (BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_ANA_CHGR_END_V(_X_)                          ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_ANA_CHG_PUMP_V(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_ANA_CHGR_PD_RTCCLR                            ( BIT(1) )
#define BIT_ANA_CHGR_PD_RTCSET                            ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_CHGR_CTRL1 */
#define BIT_ANA_DP_DM_SW_EN                               ( BIT(15) )
#define BITS_ANA_CHGR_CC_I(_X_)                           ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_ANA_VBAT_OVP_V(_X_)                          ( (_X_) << 6 & (BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_VCHG_OVP_V(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_ANA_APB_IF_CHGR_CTRL2 */
#define BITS_ANA_CHG_PUMP_CAL(_X_)                        ( (_X_) << 9 & (BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_ANA_CHG_PUMP_PD                               ( BIT(8) )
#define BIT_ANA_CHGR_CC_EN                                ( BIT(1) )
#define BIT_ANA_RECHG                                     ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_CHGR_STATUS */
#define BIT_ANA_CHG_DET_DONE                              ( BIT(11) )
#define BIT_ANA_DP_LOW                                    ( BIT(10) )
#define BIT_ANA_DCP_DET                                   ( BIT(9) )
#define BIT_ANA_CHG_DET                                   ( BIT(8) )
#define BIT_ANA_SDP_INT                                   ( BIT(7) )
#define BIT_ANA_DCP_INT                                   ( BIT(6) )
#define BIT_ANA_CDP_INT                                   ( BIT(5) )
#define BIT_ANA_CHGR_CV_STATUS                            ( BIT(4) )
#define BIT_ANA_CHGR_ON                                   ( BIT(3) )
#define BIT_ANA_CHGR_INT                                  ( BIT(2) )
#define BIT_ANA_VBAT_OVI                                  ( BIT(1) )
#define BIT_ANA_VCHG_OVI                                  ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_ANA_MIXED_CTRL */
#define BIT_ANA_PTEST_PD_RTCSET                           ( BIT(15) )
#define BIT_ANA_THM_CHIP_PD_FLAG                          ( BIT(8) )
#define BIT_ANA_THM_CHIP_PD_FLAG_CLR                      ( BIT(7) )
#define BITS_ANA_THM_CAL_SEL(_X_)                         ( (_X_) << 5 & (BIT(5)|BIT(6)) )
#define BIT_ANA_THM_AUTO_PD_EN                            ( BIT(4) )
#define BIT_ANA_BG_LP_EN                                  ( BIT(3) )
#define BITS_ANA_UVHO_T(_X_)                              ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_ANA_UVHO_EN                                   ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_PWR_XTL_EN0 */
#define BIT_ANA_LDO_XTL_EN                                ( BIT(15) )
#define BIT_ANA_LDO_RF1_EXT_XTL2_EN                       ( BIT(11) )
#define BIT_ANA_LDO_RF1_EXT_XTL1_EN                       ( BIT(10) )
#define BIT_ANA_LDO_RF1_EXT_XTL0_EN                       ( BIT(9) )
#define BIT_ANA_LDO_RF1_XTL2_EN                           ( BIT(8) )
#define BIT_ANA_LDO_RF1_XTL1_EN                           ( BIT(7) )
#define BIT_ANA_LDO_RF1_XTL0_EN                           ( BIT(6) )
#define BIT_ANA_LDO_RF0_EXT_XTL2_EN                       ( BIT(5) )
#define BIT_ANA_LDO_RF0_EXT_XTL1_EN                       ( BIT(4) )
#define BIT_ANA_LDO_RF0_EXT_XTL0_EN                       ( BIT(3) )
#define BIT_ANA_LDO_RF0_XTL2_EN                           ( BIT(2) )
#define BIT_ANA_LDO_RF0_XTL1_EN                           ( BIT(1) )
#define BIT_ANA_LDO_RF0_XTL0_EN                           ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_PWR_XTL_EN1 */
#define BIT_ANA_LDO_VDD25_EXT_XTL2_EN                     ( BIT(11) )
#define BIT_ANA_LDO_VDD25_EXT_XTL1_EN                     ( BIT(10) )
#define BIT_ANA_LDO_VDD25_EXT_XTL0_EN                     ( BIT(9) )
#define BIT_ANA_LDO_VDD25_XTL2_EN                         ( BIT(8) )
#define BIT_ANA_LDO_VDD25_XTL1_EN                         ( BIT(7) )
#define BIT_ANA_LDO_VDD25_XTL0_EN                         ( BIT(6) )
#define BIT_ANA_LDO_RF2_EXT_XTL2_EN                       ( BIT(5) )
#define BIT_ANA_LDO_RF2_EXT_XTL1_EN                       ( BIT(4) )
#define BIT_ANA_LDO_RF2_EXT_XTL0_EN                       ( BIT(3) )
#define BIT_ANA_LDO_RF2_XTL2_EN                           ( BIT(2) )
#define BIT_ANA_LDO_RF2_XTL1_EN                           ( BIT(1) )
#define BIT_ANA_LDO_RF2_XTL0_EN                           ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_PWR_XTL_EN2 */
#define BIT_ANA_LDO_AVDD18_EXT_XTL2_EN                    ( BIT(11) )
#define BIT_ANA_LDO_AVDD18_EXT_XTL1_EN                    ( BIT(10) )
#define BIT_ANA_LDO_AVDD18_EXT_XTL0_EN                    ( BIT(9) )
#define BIT_ANA_LDO_AVDD18_XTL2_EN                        ( BIT(8) )
#define BIT_ANA_LDO_AVDD18_XTL1_EN                        ( BIT(7) )
#define BIT_ANA_LDO_AVDD18_XTL0_EN                        ( BIT(6) )
#define BIT_ANA_LDO_SIM2_EXT_XTL2_EN                      ( BIT(5) )
#define BIT_ANA_LDO_SIM2_EXT_XTL1_EN                      ( BIT(4) )
#define BIT_ANA_LDO_SIM2_EXT_XTL0_EN                      ( BIT(3) )
#define BIT_ANA_LDO_SIM2_XTL2_EN                          ( BIT(2) )
#define BIT_ANA_LDO_SIM2_XTL1_EN                          ( BIT(1) )
#define BIT_ANA_LDO_SIM2_XTL0_EN                          ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_PWR_XTL_EN3 */
#define BIT_ANA_DCDC_BG_EXT_XTL2_EN                       ( BIT(11) )
#define BIT_ANA_DCDC_BG_EXT_XTL1_EN                       ( BIT(10) )
#define BIT_ANA_DCDC_BG_EXT_XTL0_EN                       ( BIT(9) )
#define BIT_ANA_DCDC_BG_XTL2_EN                           ( BIT(8) )
#define BIT_ANA_DCDC_BG_XTL1_EN                           ( BIT(7) )
#define BIT_ANA_DCDC_BG_XTL0_EN                           ( BIT(6) )
#define BIT_ANA_BG_EXT_XTL2_EN                            ( BIT(5) )
#define BIT_ANA_BG_EXT_XTL1_EN                            ( BIT(4) )
#define BIT_ANA_BG_EXT_XTL0_EN                            ( BIT(3) )
#define BIT_ANA_BG_XTL2_EN                                ( BIT(2) )
#define BIT_ANA_BG_XTL1_EN                                ( BIT(1) )
#define BIT_ANA_BG_XTL0_EN                                ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_PWR_XTL_EN4 */
#define BIT_ANA_DCDC_WRF_XTL2_EN                          ( BIT(14) )
#define BIT_ANA_DCDC_WRF_XTL1_EN                          ( BIT(13) )
#define BIT_ANA_DCDC_WRF_XTL0_EN                          ( BIT(12) )
#define BIT_ANA_DCDC_WPA_XTL2_EN                          ( BIT(11) )
#define BIT_ANA_DCDC_WPA_XTL1_EN                          ( BIT(10) )
#define BIT_ANA_DCDC_WPA_XTL0_EN                          ( BIT(9) )
#define BIT_ANA_DCDC_MEM_XTL2_EN                          ( BIT(8) )
#define BIT_ANA_DCDC_MEM_XTL1_EN                          ( BIT(7) )
#define BIT_ANA_DCDC_MEM_XTL0_EN                          ( BIT(6) )
#define BIT_ANA_DCDC_GEN_XTL2_EN                          ( BIT(5) )
#define BIT_ANA_DCDC_GEN_XTL1_EN                          ( BIT(4) )
#define BIT_ANA_DCDC_GEN_XTL0_EN                          ( BIT(3) )
#define BIT_ANA_DCDC_CORE_XTL2_EN                         ( BIT(2) )
#define BIT_ANA_DCDC_CORE_XTL1_EN                         ( BIT(1) )
#define BIT_ANA_DCDC_CORE_XTL0_EN                         ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_PWR_XTL_EN5 */
#define BIT_ANA_DCDC_WRF_EXT_XTL2_EN                      ( BIT(14) )
#define BIT_ANA_DCDC_WRF_EXT_XTL1_EN                      ( BIT(13) )
#define BIT_ANA_DCDC_WRF_EXT_XTL0_EN                      ( BIT(12) )
#define BIT_ANA_DCDC_WPA_EXT_XTL2_EN                      ( BIT(11) )
#define BIT_ANA_DCDC_WPA_EXT_XTL1_EN                      ( BIT(10) )
#define BIT_ANA_DCDC_WPA_EXT_XTL0_EN                      ( BIT(9) )
#define BIT_ANA_DCDC_MEM_EXT_XTL2_EN                      ( BIT(8) )
#define BIT_ANA_DCDC_MEM_EXT_XTL1_EN                      ( BIT(7) )
#define BIT_ANA_DCDC_MEM_EXT_XTL0_EN                      ( BIT(6) )
#define BIT_ANA_DCDC_GEN_EXT_XTL2_EN                      ( BIT(5) )
#define BIT_ANA_DCDC_GEN_EXT_XTL1_EN                      ( BIT(4) )
#define BIT_ANA_DCDC_GEN_EXT_XTL0_EN                      ( BIT(3) )
#define BIT_ANA_DCDC_CORE_EXT_XTL2_EN                     ( BIT(2) )
#define BIT_ANA_DCDC_CORE_EXT_XTL1_EN                     ( BIT(1) )
#define BIT_ANA_DCDC_CORE_EXT_XTL0_EN                     ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_ANA_STATUS */
#define BIT_ANA_BONDOPT6                                  ( BIT(6) )
#define BIT_ANA_BONDOPT5                                  ( BIT(5) )
#define BIT_ANA_BONDOPT4                                  ( BIT(4) )
#define BIT_ANA_BONDOPT3                                  ( BIT(3) )
#define BIT_ANA_BONDOPT2                                  ( BIT(2) )
#define BIT_ANA_BONDOPT1                                  ( BIT(1) )
#define BIT_ANA_BONDOPT0                                  ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_POR_RST_MONITOR */
#define BITS_ANA_POR_RST_MONITOR(_X_)                     ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_WDG_RST_MONITOR */
#define BITS_ANA_WDG_RST_MONITOR(_X_)                     ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_POR_PIN_RST_MONITOR */
#define BITS_ANA_POR_PIN_RST_MONITOR(_X_)                 ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_POR_SRC_FLAG */
#define BIT_ANA_POR_SW_FORCE_ON                           ( BIT(15) )
#define BITS_ANA_POR_SRC_FLAG(_X_)                        ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )

/* bits definitions for register REG_ANA_APB_IF_POR_7S_CTRL */
#define BIT_ANA_PBINT_7S_FLAG_CLR                         ( BIT(15) )
#define BIT_ANA_EXT_RSTN_FLAG_CLR                         ( BIT(14) )
#define BIT_ANA_CHGR_INT_FLAG_CLR                         ( BIT(13) )
#define BIT_ANA_PBINT2_FLAG_CLR                           ( BIT(12) )
#define BIT_ANA_PBINT_FLAG_CLR                            ( BIT(11) )
#define BIT_ANA_PBINT_7S_RST_SWMODE_RTCSTS                ( BIT(10) )
#define BIT_ANA_PBINT_7S_RST_SWMODE_RTCCLR                ( BIT(9) )
#define BIT_ANA_PBINT_7S_RST_SWMODE_RTCSET                ( BIT(8) )
#define BITS_ANA_PBINT_7S_RST_THRESHOLD(_X_)              ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ANA_PBINT_7S_RST_DISABLE                      ( BIT(3) )
#define BIT_ANA_PBINT_7S_RST_MODE_RTCSTS                  ( BIT(2) )
#define BIT_ANA_PBINT_7S_RST_MODE_RTCCLR                  ( BIT(1) )
#define BIT_ANA_PBINT_7S_RST_MODE_RTCSET                  ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_INT_DEBUG */
#define BIT_ANA_OTP_INT_DEB                               ( BIT(10) )
#define BIT_ANA_THM_INT_DEB                               ( BIT(9) )
#define BIT_ANA_AUD_PROT_INT_DEB                          ( BIT(8) )
#define BIT_ANA_AUD_HEADBTN_INT_DEB                       ( BIT(7) )
#define BIT_ANA_EIC_INT_DEB                               ( BIT(6) )
#define BIT_ANA_FGU_INT_DEB                               ( BIT(5) )
#define BIT_ANA_WDG_INT_DEB                               ( BIT(4) )
#define BIT_ANA_RTC_INT_DEB                               ( BIT(3) )
#define BIT_ANA_GPIO_INT_DEB                              ( BIT(2) )
#define BIT_ANA_ADC_INT_DEB                               ( BIT(1) )
#define BIT_ANA_INT_DEBUG_EN                              ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_GPI_DEBUG */
#define BIT_ANA_HEAD_INSERT2_DEB                          ( BIT(9) )
#define BIT_ANA_HEAD_INSERT_DEB                           ( BIT(8) )
#define BIT_ANA_HEAD_BUTTON_DEB                           ( BIT(7) )
#define BIT_ANA_PBINT2_DEB                                ( BIT(6) )
#define BIT_ANA_PBINT_DEB                                 ( BIT(5) )
#define BIT_ANA_VCHG_OVI_DEB                              ( BIT(4) )
#define BIT_ANA_VBAT_OVI_DEB                              ( BIT(3) )
#define BIT_ANA_CHGR_INT_DEB                              ( BIT(2) )
#define BIT_ANA_GPI_DEBUG_EN                              ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_HWRST_RTC */
#define BITS_ANA_HWRST_RTC_REG_STS(_X_)                   ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ANA_HWRST_RTC_REG_SET(_X_)                   ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_ANA_APB_IF_CHIP_ID_LOW */
#define BITS_ANA_CHIP_ID_LOW(_X_)                         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_CHIP_ID_HIGH */
#define BITS_ANA_CHIP_ID_HIGH(_X_)                        ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_ARM_MF_REG */
#define BITS_ANA_ARM_MF_REG(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_AFUSE_CTRL */
#define BIT_ANA_AFUSE_READ_REQ                            ( BIT(7) )
#define BITS_ANA_AFUSE_READ_DLY(_X_)                      ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register REG_ANA_APB_IF_AFUSE_OUT0 */
#define BITS_ANA_AFUSE_OUT0(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_AFUSE_OUT1 */
#define BITS_ANA_AFUSE_OUT1(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_AFUSE_OUT2 */
#define BITS_ANA_AFUSE_OUT2(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_AFUSE_OUT3 */
#define BITS_ANA_AFUSE_OUT3(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_CA_CTRL4 */
#define BIT_ANA_EXT_RSTN_PD_EN                            ( BIT(14) )
#define BIT_ANA_PB_7S_RST_PD_EN                           ( BIT(13) )
#define BIT_ANA_WDG_RST_PD_EN                             ( BIT(12) )
#define BIT_ANA_SW_RST_EMMCIO_PD_EN                       ( BIT(11) )
#define BIT_ANA_SW_RST_EMMCCORE_PD_EN                     ( BIT(10) )
#define BITS_ANA_SW_RST_PD_THRESHOLD(_X_)                 ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )

/* bits definitions for register REG_ANA_APB_IF_MCU_WR_PROT_VALUE */
#define BIT_ANA_MCU_WR_PROT                               ( BIT(15) )
#define BITS_ANA_MCU_WR_PROT_VALUE(_X_)                   ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )

/* bits definitions for register REG_ANA_APB_IF_MP_PWR_CTRL0 */
#define BIT_ANA_LDO_SIM1_EXT_XTL2_EN                      ( BIT(15) )
#define BIT_ANA_LDO_SIM1_EXT_XTL1_EN                      ( BIT(14) )
#define BIT_ANA_LDO_SIM1_EXT_XTL0_EN                      ( BIT(13) )
#define BIT_ANA_LDO_SIM1_XTL2_EN                          ( BIT(12) )
#define BIT_ANA_LDO_SIM1_XTL1_EN                          ( BIT(11) )
#define BIT_ANA_LDO_SIM1_XTL0_EN                          ( BIT(10) )
#define BIT_ANA_LDO_SIM0_EXT_XTL2_EN                      ( BIT(9) )
#define BIT_ANA_LDO_SIM0_EXT_XTL1_EN                      ( BIT(8) )
#define BIT_ANA_LDO_SIM0_EXT_XTL0_EN                      ( BIT(7) )
#define BIT_ANA_LDO_SIM0_XTL2_EN                          ( BIT(6) )
#define BIT_ANA_LDO_SIM0_XTL1_EN                          ( BIT(5) )
#define BIT_ANA_LDO_SIM0_XTL0_EN                          ( BIT(4) )
#define BIT_ANA_PWR_OFF_SEQ_EN                            ( BIT(3) )
#define BIT_ANA_DCDC_CORE_SLP_OUT_STEP_EN                 ( BIT(1) )
#define BIT_ANA_DCDC_CORE_SLP_IN_STEP_EN                  ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_MP_PWR_CTRL1 */
#define BITS_ANA_DCDC_CORE_CTL_SLP_STEP5(_X_)             ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)) )
#define BITS_ANA_DCDC_CORE_CTL_SLP_STEP4(_X_)             ( (_X_) << 9 & (BIT(9)|BIT(10)|BIT(11)) )
#define BITS_ANA_DCDC_CORE_CTL_SLP_STEP3(_X_)             ( (_X_) << 6 & (BIT(6)|BIT(7)|BIT(8)) )
#define BITS_ANA_DCDC_CORE_CTL_SLP_STEP2(_X_)             ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_ANA_DCDC_CORE_CTL_SLP_STEP1(_X_)             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_ANA_APB_IF_MP_PWR_CTRL2 */
#define BITS_ANA_DCDC_CORE_CAL_SLP_STEP3(_X_)             ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_ANA_DCDC_CORE_CAL_SLP_STEP2(_X_)             ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_DCDC_CORE_CAL_SLP_STEP1(_X_)             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_MP_PWR_CTRL3 */
#define BITS_ANA_DCDC_ARM_STBOP(_X_)                      ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_ANA_DCDC_CORE_STBOP(_X_)                     ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_ANA_DCDC_CORE_CAL_SLP_STEP5(_X_)             ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_DCDC_CORE_CAL_SLP_STEP4(_X_)             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_MP_LDO_CTRL */
#define BITS_ANA_LDOD_LP_CAL(_X_)                         ( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_ANA_LDOA_LP_CAL(_X_)                         ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_ANA_LDODCDC_LP_CAL(_X_)                      ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_MP_CHG_CTRL */
#define BIT_ANA_CHGR_INT_EN                               ( BIT(8) )
#define BIT_ANA_CHGR_DRV                                  ( BIT(5) )
#define BIT_ANA_CHGR_OSC                                  ( BIT(4) )
#define BITS_ANA_CHGR_DPM(_X_)                            ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_ANA_CHGR_ITERM(_X_)                          ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ANA_APB_IF_MP_MISC_CTRL */
#define BITS_ANA_LDO_RF1_V_BOND(_X_)                      ( (_X_) << 11 & (BIT(11)|BIT(12)) )
#define BITS_ANA_LDO_VDD25_V_BOND(_X_)                    ( (_X_) << 9 & (BIT(9)|BIT(10)) )
#define BITS_ANA_DCDC_ARM_CTL_BOND(_X_)                   ( (_X_) << 6 & (BIT(6)|BIT(7)|BIT(8)) )
#define BITS_ANA_DCDC_CORE_CTL_BOND(_X_)                  ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BIT_ANA_DCOFFSET_EN                               ( BIT(2) )
#define BIT_ANA_CHOP_EN                                   ( BIT(1) )
#define BIT_ANA_WHTLED_EXT_RES_CTRL                       ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_MP_DCDC_DEDT_CTRL */
#define BITS_ANA_DCDC_WPA_DEDT_EN(_X_)                    ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_ANA_DCDC_WRF_DEDT_EN(_X_)                    ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ANA_DCDC_GEN_DEDT_EN(_X_)                    ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_ANA_DCDC_MEM_DEDT_EN(_X_)                    ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_ANA_DCDC_ARM_DEDT_EN(_X_)                    ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_ANA_DCDC_CORE_DEDT_EN(_X_)                   ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ANA_APB_IF_CA_CTRL0 */
#define BITS_ANA_SD_CLK_P(_X_)                            ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BIT_ANA_SD_CHOP_CAP_EN                            ( BIT(13) )
#define BIT_ANA_LDO_CAMIO_V_B2                            ( BIT(12) )
#define BIT_ANA_LDO_CAMIO_EXT_XTL2_EN                     ( BIT(11) )
#define BIT_ANA_LDO_CAMIO_EXT_XTL1_EN                     ( BIT(10) )
#define BIT_ANA_LDO_CAMIO_EXT_XTL0_EN                     ( BIT(9) )
#define BIT_ANA_LDO_CAMIO_XTL2_EN                         ( BIT(8) )
#define BIT_ANA_LDO_CAMIO_XTL1_EN                         ( BIT(7) )
#define BIT_ANA_LDO_CAMIO_XTL0_EN                         ( BIT(6) )
#define BIT_ANA_LDO_CAMD_EXT_XTL2_EN                      ( BIT(5) )
#define BIT_ANA_LDO_CAMD_EXT_XTL1_EN                      ( BIT(4) )
#define BIT_ANA_LDO_CAMD_EXT_XTL0_EN                      ( BIT(3) )
#define BIT_ANA_LDO_CAMD_XTL2_EN                          ( BIT(2) )
#define BIT_ANA_LDO_CAMD_XTL1_EN                          ( BIT(1) )
#define BIT_ANA_LDO_CAMD_XTL0_EN                          ( BIT(0) )

/* bits definitions for register REG_ANA_APB_IF_CA_CTRL1 */
#define BITS_ANA_SMPL_MODE(_X_)                           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ANA_APB_IF_CA_CTRL2 */
#define BIT_ANA_SMPL_PWR_ON_FLAG                          ( BIT(15) )
#define BIT_ANA_SMPL_MODE_WR_ACK_FLAG                     ( BIT(14) )
#define BIT_ANA_SMPL_PWR_ON_FLAG_CLR                      ( BIT(13) )
#define BIT_ANA_SMPL_MODE_WR_ACK_FLAG_CLR                 ( BIT(12) )
#define BIT_ANA_SMPL_PWR_ON_SET                           ( BIT(11) )
#define BIT_ANA_SLP_LDOKPLED_PD_EN                        ( BIT(9) )
#define BIT_ANA_LDO_KPLED_PD                              ( BIT(8) )
#define BITS_ANA_LDO_KPLED_V(_X_)                         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_ANA_APB_IF_CA_CTRL3 */
#define BITS_ANA_VBAT_CRASH_V(_X_)                        ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BIT_ANA_PBINT_7S_AUTO_ON_EN_CLR                   ( BIT(13) )
#define BIT_ANA_PBINT_7S_AUTO_ON_EN_SET                   ( BIT(12) )
#define BIT_ANA_VIBR_LDO_EN                               ( BIT(10) )
#define BIT_ANA_SLP_LDOVIBR_PD_EN                         ( BIT(9) )
#define BIT_ANA_LDO_VIBR_PD                               ( BIT(8) )
#define BITS_ANA_LDO_VIBR_V(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_CORE_ADI */
#define BITS_ANA_DCDC_CORE_CTL_SW_ADI(_X_)                ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)) )
#define BITS_ANA_DCDC_CORE_CAL_SW_ADI(_X_)                ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_ARM_ADI */
#define BITS_ANA_DCDC_ARM_CTL_ADI(_X_)                    ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)) )
#define BITS_ANA_DCDC_ARM_CAL_ADI(_X_)                    ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_MEM_ADI */
#define BIT_ANA_DCDC_MEM_CTL_ADI                          ( BIT(5) )
#define BITS_ANA_DCDC_MEM_CAL_ADI(_X_)                    ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_GEN_ADI */
#define BITS_ANA_DCDC_GEN_CTL_ADI(_X_)                    ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)) )
#define BITS_ANA_DCDC_GEN_CAL_ADI(_X_)                    ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_WRF_ADI */
#define BITS_ANA_DCDC_WRF_CTL_ADI(_X_)                    ( (_X_) << 5 & (BIT(5)|BIT(6)) )
#define BITS_ANA_DCDC_WRF_CAL_ADI(_X_)                    ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_WPA_ADI */
#define BITS_ANA_DCDC_WPA_CAL_ADI(_X_)                    ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_ANA_APB_IF_DCDC_WPA_DCM_ADI */
#define BIT_ANA_DCDC_WPA_DCM_ADI                          ( BIT(0) )

#endif
