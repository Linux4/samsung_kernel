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

#ifndef __ANA_REGS_GLB_H__
#define __ANA_REGS_GLB_H__

#ifndef __SCI_GLB_REGS_H__
#error  "Don't include this file directly, include <mach/sci_glb_regs.h>"
#endif

#define ANA_REGS_GLB

/* registers definitions for controller ANA_REGS_GLB */
#define ANA_REG_GLB_ANA_APB_CLK_EN      SCI_ADDR(ANA_REGS_GLB_BASE, 0x000)
#define ANA_REG_GLB_ANA_APB_ARM_RST     SCI_ADDR(ANA_REGS_GLB_BASE, 0x004)
#define ANA_REG_GLB_LDO_PD_SET          SCI_ADDR(ANA_REGS_GLB_BASE, 0x008)
#define ANA_REG_GLB_LDO_PD_RST          SCI_ADDR(ANA_REGS_GLB_BASE, 0x00c)
#define ANA_REG_GLB_LDO_PD_CTRL0        SCI_ADDR(ANA_REGS_GLB_BASE, 0x010)
#define ANA_REG_GLB_LDO_PD_CTRL1        SCI_ADDR(ANA_REGS_GLB_BASE, 0x014)
#define ANA_REG_GLB_LDO_VCTRL0          SCI_ADDR(ANA_REGS_GLB_BASE, 0x018)
#define ANA_REG_GLB_LDO_VCTRL1          SCI_ADDR(ANA_REGS_GLB_BASE, 0x01c)
#define ANA_REG_GLB_LDO_VCTRL2          SCI_ADDR(ANA_REGS_GLB_BASE, 0x020)
#define ANA_REG_GLB_LDO_VCTRL3          SCI_ADDR(ANA_REGS_GLB_BASE, 0x024)
#define ANA_REG_GLB_LDO_VCTRL4          SCI_ADDR(ANA_REGS_GLB_BASE, 0x028)
#define ANA_REG_GLB_LDO_SLP_CTRL0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x030)
#define ANA_REG_GLB_LDO_SLP_CTRL1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x034)
#define ANA_REG_GLB_LDO_SLP_CTRL2       SCI_ADDR(ANA_REGS_GLB_BASE, 0x038)
#define ANA_REG_GLB_LDO_SLP_CTRL3       SCI_ADDR(ANA_REGS_GLB_BASE, 0x03c)
#define ANA_REG_GLB_DCDC_CTRL0          SCI_ADDR(ANA_REGS_GLB_BASE, 0x040)
#define ANA_REG_GLB_DCDC_CTRL1          SCI_ADDR(ANA_REGS_GLB_BASE, 0x044)
#define ANA_REG_GLB_DCDC_CTRL2          SCI_ADDR(ANA_REGS_GLB_BASE, 0x048)
#define ANA_REG_GLB_DCDC_CTRL_DS        SCI_ADDR(ANA_REGS_GLB_BASE, 0x04c)
#define ANA_REG_GLB_DCDC_CTRL_CAL       SCI_ADDR(ANA_REGS_GLB_BASE, 0x050)
#define ANA_REG_GLB_PLL_CTRL            SCI_ADDR(ANA_REGS_GLB_BASE, 0x054)
#define ANA_REG_GLB_APLLMN              SCI_ADDR(ANA_REGS_GLB_BASE, 0x058)
#define ANA_REG_GLB_APLLWAIT            SCI_ADDR(ANA_REGS_GLB_BASE, 0x05c)
#define ANA_REG_GLB_RTC_CTRL            SCI_ADDR(ANA_REGS_GLB_BASE, 0x060)
#define ANA_REG_GLB_BUF26M_CTRL         SCI_ADDR(ANA_REGS_GLB_BASE, 0x064)
#define ANA_REG_GLB_CHGR_CTRL0          SCI_ADDR(ANA_REGS_GLB_BASE, 0x068)
#define ANA_REG_GLB_CHGR_CTRL1          SCI_ADDR(ANA_REGS_GLB_BASE, 0x06c)
#define ANA_REG_GLB_LED_CTRL            SCI_ADDR(ANA_REGS_GLB_BASE, 0x070)
#define ANA_REG_GLB_VIBRATOR_CTRL0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x074)
#define ANA_REG_GLB_VIBRATOR_CTRL1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x078)
#define ANA_REG_GLB_ARM_AUD_CLK_RST     SCI_ADDR(ANA_REGS_GLB_BASE, 0x07c)
#define ANA_REG_GLB_ANA_MIXED_CTRL      SCI_ADDR(ANA_REGS_GLB_BASE, 0x080)
#define ANA_REG_GLB_ANA_STATUS          SCI_ADDR(ANA_REGS_GLB_BASE, 0x084)
#define ANA_REG_GLB_RST_STATUS          SCI_ADDR(ANA_REGS_GLB_BASE, 0x088)
#define ANA_REG_GLB_MCU_WR_PROT         SCI_ADDR(ANA_REGS_GLB_BASE, 0x08c)
#define ANA_REG_GLB_VIBR_WR_PROT        SCI_ADDR(ANA_REGS_GLB_BASE, 0x090)
#define ANA_REG_GLB_INT_GPI_DEBUG       SCI_ADDR(ANA_REGS_GLB_BASE, 0x094)
#define ANA_REG_GLB_HWRST_RTC           SCI_ADDR(ANA_REGS_GLB_BASE, 0x098)
#define ANA_REG_GLB_DCDCOTP_CTRL        SCI_ADDR(ANA_REGS_GLB_BASE, 0x09c)
#define ANA_REG_GLB_POR_SRC_STAT        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0a0)
#define ANA_REG_GLB_POR_SRC_FLAG_CLR    SCI_ADDR(ANA_REGS_GLB_BASE, 0x0a4)
#define ANA_REG_GLB_HW_REBOOT_CTRL      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0a8)
#define ANA_REG_GLB_DCDCARM_CTRL0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0ac)
#define ANA_REG_GLB_DCDCARM_CTRL1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0b0)
#define ANA_REG_GLB_DCDCARM_CTRL2       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0b4)
#define ANA_REG_GLB_DCDCARM_CTRL_CAL    SCI_ADDR(ANA_REGS_GLB_BASE, 0x0b8)
#define ANA_REG_GLB_DCDCMEM_CTRL0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0bc)
#define ANA_REG_GLB_DCDCMEM_CTRL1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0c0)
#define ANA_REG_GLB_DCDCMEM_CTRL2       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0c4)
#define ANA_REG_GLB_DCDCMEM_CTRL_CAL    SCI_ADDR(ANA_REGS_GLB_BASE, 0x0c8)
#define ANA_REG_GLB_DDR2_BUF_CTRL0_DS   SCI_ADDR(ANA_REGS_GLB_BASE, 0x0cc)
#define ANA_REG_GLB_DDR2_BUF_CTRL1_DS   SCI_ADDR(ANA_REGS_GLB_BASE, 0x0d0)
#define ANA_REG_GLB_EFS_PROT            SCI_ADDR(ANA_REGS_GLB_BASE, 0x0d4)
#define ANA_REG_GLB_EFS_CTRL            SCI_ADDR(ANA_REGS_GLB_BASE, 0x0d8)
#define ANA_REG_GLB_DCDCLDO_CTRL0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0dc)
#define ANA_REG_GLB_DCDCLDO_CTRL1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0e0)
#define ANA_REG_GLB_DCDCLDO_CTRL2       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0e4)
#define ANA_REG_GLB_DCDCLDO_CTRL_CAL    SCI_ADDR(ANA_REGS_GLB_BASE, 0x0e8)
#define ANA_REG_GLB_AFUSE_CTRL          SCI_ADDR(ANA_REGS_GLB_BASE, 0x0ec)
#define ANA_REG_GLB_AFUSE_OUT_LOW       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0f0)
#define ANA_REG_GLB_AFUSE_OUT_HIGH      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0f4)
#define ANA_REG_GLB_CHIP_ID_LOW         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0f8)
#define ANA_REG_GLB_CHIP_ID_HIGH        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0fc)

/* bits definitions for register ANA_REG_GLB_ANA_APB_CLK_EN */
#define BIT_ANA_CHGRWDG_EB              ( BIT(15) )
#define BIT_ANA_CLK_AUXAD_EN            ( BIT(14) )
#define BIT_ANA_CLK_AUXADC_EN           ( BIT(13) )
#define BIT_ANA_RTC_TPC_EB              ( BIT(12) )
#define BIT_ANA_RTC_EIC_EB              ( BIT(11) )
#define BIT_ANA_RTC_WDG_EB              ( BIT(10) )
#define BIT_ANA_RTC_RTC_EB              ( BIT(9) )
#define BIT_ANA_RTC_ARCH_EB             ( BIT(8) )
#define BIT_ANA_PINREG_EB               ( BIT(7) )
#define BIT_ANA_GPIO_EB                 ( BIT(6) )
#define BIT_ANA_ADC_EB                  ( BIT(5) )
#define BIT_ANA_TPC_EB                  ( BIT(4) )
#define BIT_ANA_EIC_EB                  ( BIT(3) )
#define BIT_ANA_WDG_EB                  ( BIT(2) )
#define BIT_ANA_RTC_EB                  ( BIT(1) )
#define BIT_ANA_APB_ARCH_EB             ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_ANA_APB_ARM_RST */
#define BIT_ANA_GPIO_SOFT_RST           ( BIT(7) )
#define BIT_ANA_EIC_SOFT_RST            ( BIT(6) )
#define BIT_ANA_TPC_SOFT_RST            ( BIT(5) )
#define BIT_ANA_ADC_SOFT_RST            ( BIT(4) )
#define BIT_ANA_WDG_SOFT_RST            ( BIT(3) )
#define BIT_ANA_CHGWDG_SOFT_RST         ( BIT(2) )
#define BIT_ANA_RTC_SOFT_RST            ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_PD_SET */
#define BIT_DCDC_LDO_PD                 ( BIT(9) )
#define BIT_LDO_VDD25_PD                ( BIT(8) )
#define BIT_LDO_VDD18_PD                ( BIT(7) )
#define BIT_LDO_VDD28_PD                ( BIT(6) )
#define BIT_LDO_VDDAVDDBB_PD            ( BIT(5) )
#define BIT_LDO_RF_PD                   ( BIT(4) )
#define BIT_DCDC_MEM_PD                 ( BIT(3) )
#define BIT_DCDC_ARM_PD                 ( BIT(2) )
#define BIT_DCDC_PD                     ( BIT(1) )
#define BIT_BG_PD                       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_PD_RST */
#define BIT_DCDC_LDO_RST                ( BIT(9) )
#define BIT_LDO_VDD25_RST               ( BIT(8) )
#define BIT_LDO_VDD18_RST               ( BIT(7) )
#define BIT_LDO_VDD28_RST               ( BIT(6) )
#define BIT_LDO_VDDAVDDBB_RST           ( BIT(5) )
#define BIT_LDO_RF_RST                  ( BIT(4) )
#define BIT_DCDC_MEM_RST                ( BIT(3) )
#define BIT_DCDC_ARM_RST                ( BIT(2) )
#define BIT_DCDC_RST                    ( BIT(1) )
#define BIT_BG_RST                      ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_PD_CTRL0 */
#define BIT_LDO_BP_CAMMOT_RST           ( BIT(15) )
#define BIT_LDO_BPCAMMOT                ( BIT(14) )
#define BIT_LDO_BPCAMA_RST              ( BIT(13) )
#define BIT_LDO_BPCAMA                  ( BIT(12) )
#define BIT_LDO_BPCAMIO_RST             ( BIT(11) )
#define BIT_LDO_BPCAMIO                 ( BIT(10) )
#define BIT_LDO_BPCAMCORE_RST           ( BIT(9) )
#define BIT_LDO_BPCAMCORE               ( BIT(8) )
#define BIT_LDO_BPSIM1_RST              ( BIT(7) )
#define BIT_LDO_BPSIM1                  ( BIT(6) )
#define BIT_LDO_BPSIM0_RST              ( BIT(5) )
#define BIT_LDO_BPSIM0                  ( BIT(4) )
#define BIT_LDO_BPUSB_RST               ( BIT(1) )
#define BIT_LDO_BPUSB                   ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_PD_CTRL1 */
#define BIT_LDO_BPCMMB1V2_RST           ( BIT(13) )
#define BIT_LDO_BPCMMB1V2               ( BIT(12) )
#define BIT_LDO_BPCMMB1P8_RST           ( BIT(11) )
#define BIT_LDO_BPCMMB1P8               ( BIT(10) )
#define BIT_LDO_BPVDD3V_RST             ( BIT(7) )
#define BIT_LDO_BPVDD3V                 ( BIT(6) )
#define BIT_LDO_BPSD3_RST               ( BIT(5) )
#define BIT_LDO_BPSD3                   ( BIT(4) )
#define BIT_LDO_BPSD1_RST               ( BIT(3) )
#define BIT_LDO_BPSD1                   ( BIT(2) )
#define BIT_LDO_BPSD0_RST               ( BIT(1) )
#define BIT_LDO_BPSD0                   ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_VCTRL0 */
#define BITS_LDO_AVDDBB_VCTL(_x_)       ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_LDO_RF_VCTL(_x_)           ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LDO_RTC_VCTL(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register ANA_REG_GLB_LDO_VCTRL1 */
#define BITS_LDO_USB_VCTL(_x_)          ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_LDO_VDD3V_VCTL(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_LDO_SIM1_VCTL(_x_)         ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LDO_SIM0_VCTL(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register ANA_REG_GLB_LDO_VCTRL2 */
#define BITS_LDO_CAMMOT_VCTL(_x_)       ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_LDO_CAMA_VCTL(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_LDO_CAMIO_VCTL(_x_)        ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LDO_CAMCORE_VCTL(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register ANA_REG_GLB_LDO_VCTRL3 */
#define BITS_LDO_SD0_VCTL(_x_)          ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_LDO_VDD25_VCTL(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_LDO_VDD18_VCTL(_x_)        ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LDO_VDD28_VCTL(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register ANA_REG_GLB_LDO_VCTRL4 */
#define BITS_LDO_CMMB1V2_VCTL(_x_)      ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_LDO_CMMB1P8_VCTL(_x_)      ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_LDO_SD3_VCTL(_x_)          ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LDO_SD1_VCTL(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register ANA_REG_GLB_LDO_SLP_CTRL0 */
#define BIT_LDOSD3_BP_EN                ( BIT(15) )
#define BIT_LDOSD1_BP_EN                ( BIT(14) )
#define BIT_LDOVDD25_BP_EN              ( BIT(13) )
#define BIT_LDOVDD18_BP_EN              ( BIT(12) )
#define BIT_LDOVDD28_BP_EN              ( BIT(11) )
#define BIT_LDOVAVDDBB_BP_EN            ( BIT(10) )
#define BIT_LDOSD0_BP_EN                ( BIT(9) )
#define BIT_LDOCAMMOT_BP_EN             ( BIT(8) )
#define BIT_LDOCAMA_BP_EN               ( BIT(7) )
#define BIT_LDOCAMIO_BP_EN              ( BIT(6) )
#define BIT_LDOCAMCORE_BP_EN            ( BIT(5) )
#define BIT_LDOUSB_BP_EN                ( BIT(4) )
#define BIT_LDOSIM1_BP_EN               ( BIT(3) )
#define BIT_LDOSIM0_BP_EN               ( BIT(2) )
#define BIT_LDORF_BP_EN                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_SLP_CTRL1 */
#define BIT_FSM_SLPPD_EN                ( BIT(15) )
#define BIT_SLP_AUDIO_AUXMICBIAS_PD_EN  ( BIT(12) )
#define BIT_SLP_AUDIO_MICBIAS_PD_EN     ( BIT(11) )
#define BIT_SLP_AUDIO_VBO_PD_EN         ( BIT(10) )
#define BIT_SLP_AUDIO_VB_PD_EN          ( BIT(9) )
#define BIT_SLP_AUDIO_BG_IBIAS_PD_EN    ( BIT(8) )
#define BIT_SLP_AUDIO_BG_PD_EN          ( BIT(7) )
#define BIT_SLP_AUDIO_VCMBUF_PD_EN      ( BIT(6) )
#define BIT_SLP_AUDIO_VCM_PD_EN         ( BIT(5) )
#define BIT_DCDC_ARM_BP_EN              ( BIT(4) )
#define BIT_LDOCMMB1P8_BP_EN            ( BIT(3) )
#define BIT_LDOCMMB1V2_BP_EN            ( BIT(2) )
#define BIT_LDOVDD3V_BP_EN              ( BIT(1) )

/* bits definitions for register ANA_REG_GLB_LDO_SLP_CTRL2 */
#define BITS_ARMDCDC_ISO_ON_NUM(_x_)    ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ARMDCDC_ISO_OFF_NUM(_x_)   ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_SLP_CTRL3 */
#define BITS_ARMDCDC_PWR_ON_DLY(_x_)    ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CTRL0 */
#define BIT_DCDC_FRECUT_RST             ( BIT(15) )
#define BIT_DCDC_FRECUT                 ( BIT(14) )
#define BIT_DCDC_PFM_RST                ( BIT(13) )
#define BIT_DCDC_PFM                    ( BIT(12) )
#define BIT_DCDC_DCM_RST                ( BIT(11) )
#define BIT_DCDC_DCM                    ( BIT(10) )
#define BIT_DCDC_DEDT_EN_RST            ( BIT(9) )
#define BIT_DCDC_DEDT_EN                ( BIT(8) )
#define BITS_DCDC_CTL_40NM_RST(_x_)     ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_DCDC_CTL_40NM(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CTRL1 */
#define BITS_DCDC_PDRSLOW_RST(_x_)      ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DCDC_PDRSLOW(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_DCDC_CL_CTRL_RST            ( BIT(7) )
#define BIT_DCDC_CL_CTRL                ( BIT(6) )
#define BIT_DCDC_BP_LP_EN_RST           ( BIT(5) )
#define BIT_DCDC_BP_LP_EN               ( BIT(4) )
#define BIT_DCDC_OSCSYCEN_SW            ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_CTRL2 */
#define BIT_DCDC_OSCSYCEN_HW_EN         ( BIT(14) )
#define BIT_DCDC_OSCSYC_DIV_EN          ( BIT(13) )
#define BITS_DCDC_OSCSYC_DIV(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_DCDC_RESERVER_RST(_x_)     ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_DCDC_RESERVER(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CTRL_DS */
#define BITS_DCDC_LVL_DLY(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DCDC_CTL_40NM_DS_RST(_x_)  ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_DCDC_CTL_40NM_DS(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CTRL_CAL */
#define BITS_DCDC_CAL_RST(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_DCDC_CAL(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_PLL_CTRL */
#define BIT_APLL_MN_WE                  ( BIT(3) )
#define BIT_APLL_PD_EN                  ( BIT(2) )
#define BIT_APLL_FORECE_PD              ( BIT(1) )
#define BIT_APLL_FORECE_PD_EN           ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_APLLMN */
#define BITS_APLLM(_x_)                 ( (_x_) << 11 & (BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_APLLN(_x_)                 ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register ANA_REG_GLB_APLLWAIT */
#define BITS_APLLWAIT(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_RTC_CTRL */
#define BITS_VBATBK_RES(_x_)            ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_VBATBK_V(_x_)              ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_32K_START_CUR(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_BUF26M_CTRL */
#define BIT_CLK26M_NORMAL_EN            ( BIT(15) )
#define BIT_SINDRV_ENA_SQUARE           ( BIT(3) )
#define BIT_SINDRV_ENA                  ( BIT(2) )
#define BITS_SINDRV_LVL(_x_)            ( (_x_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_CLIP_MODE                   ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_CHGR_CTRL0 */
#define BIT_RECHG                       ( BIT(12) )
#define BIT_CHGR_PWM_EN_RST             ( BIT(11) )
#define BIT_CHGR_PWM_EN                 ( BIT(10) )
#define BIT_CHGR_CC_EN_RST              ( BIT(1) )
#define BIT_CHGR_CC_EN                  ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_CHGR_CTRL1 */
#define BITS_CHGR_RTCCTL(_x_)           ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CHGR_CTL(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LED_CTRL */
#define BIT_KPLED_PD_RST                ( BIT(12) )
#define BIT_KPLED_PD                    ( BIT(11) )
#define BITS_KPLED_V(_x_)               ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BIT_WHTLED_PD_RST               ( BIT(6) )
#define BIT_WHTLED_PD                   ( BIT(5) )
#define BITS_WHTLED_V(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_VIBRATOR_CTRL0 */
#define BITS_VIBR_STABLE_V_B(_x_)       ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_VIBR_INIT_V_A(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_VIBR_V_BP(_x_)             ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_VIBR_PD_RST                 ( BIT(3) )
#define BIT_VIBR_PD                     ( BIT(2) )
#define BIT_VIBR_BD_EN                  ( BIT(1) )
#define BIT_RTC_VIBR_EN                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_VIBRATOR_CTRL1 */
#define BITS_VIBR_CONVERT_V_COUNT(_x_)  ( (_x_) << 0 )

/* bits definitions for register ANA_REG_GLB_ARM_AUD_CLK_RST */
#define BIT_AUD_ARM_ACC                 ( BIT(15) )
#define BIT_AUDRX_ARM_SOFT_RST          ( BIT(10) )
#define BIT_AUDTX_ARM_SOFT_RST          ( BIT(9) )
#define BIT_AUD_ARM_SOFT_RST            ( BIT(8) )
#define BIT_AUD6M5_CLK_RX_INV_ARM_ENN   ( BIT(7) )
#define BIT_AUD6M5_CLK_TX_INV_ARM_EN    ( BIT(6) )
#define BIT_AUDIF_CLK_RX_INV_ARM_EN     ( BIT(5) )
#define BIT_AUDIF_CLK_TXT_INV_ARM_EN    ( BIT(4) )
#define BIT_CLK_AUD_6M5_ARM_EN          ( BIT(3) )
#define BIT_CLK_AUDIF_ARM_EN            ( BIT(2) )
#define BIT_RTC_AUD_ARM_EN              ( BIT(1) )
#define BIT_AUD_ARM_EN                  ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_ANA_MIXED_CTRL */
#define BIT_PTEST_PD_SET                ( BIT(15) )
#define BITS_UVHO_T_RST(_x_)            ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_UVHO_T(_x_)                ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_VIBR_PWR_ERR_CLR            ( BIT(7) )
#define BIT_CLKBT_EN                    ( BIT(6) )
#define BITS_CLK_26M_REGS0(_x_)         ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_UVHO_EN_RST                 ( BIT(3) )
#define BIT_UVHO_EN                     ( BIT(2) )
#define BIT_OTP_EN_RST                  ( BIT(1) )
#define BIT_OTP_EN                      ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_ANA_STATUS */
#define BIT_VIBR_PWR_ERR                ( BIT(15) )
#define BIT_ANA_BONDOPT2                ( BIT(10) )
#define BIT_STS_VIBR_PD                 ( BIT(9) )
#define BIT_STS_WHTLED_PD               ( BIT(8) )
#define BITS_PA_OCP_FLAG(_x_)           ( (_x_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_PA_OTP_OTP(_x_)            ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_CHGR_ON                     ( BIT(3) )
#define BIT_CHGR_STDBY                  ( BIT(2) )
#define BIT_ANA_BONDOPT1                ( BIT(1) )
#define BIT_ANA_BONDOPT0                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_RST_STATUS */
#define BITS_ALL_HRST_MON(_x_)          ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_POR_HRST_MON(_x_)          ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_WDG_HRST_MON(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register ANA_REG_GLB_MCU_WR_PROT */
#define BITS_MCU_WR_PROT(_x_)           ( (_x_) << 0 )

/* bits definitions for register ANA_REG_GLB_VIBR_WR_PROT */
#define BITS_VIBR_WR_PROT(_x_)          ( (_x_) << 0 )

/* bits definitions for register ANA_REG_GLB_HWRST_RTC */
#define BITS_HWRST_RTC_REG(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HWRST_RTC_SET(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_DCDCOTP_CTRL */
#define BITS_DCDC_OTP_OPTION(_x_)       ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BIT_DCDC_OTP_INT_CLR            ( BIT(13) )
#define BIT_DCDC_OPT_STS_RTC            ( BIT(12) )
#define BIT_DCDC_OTP_EN_RST             ( BIT(9) )
#define BIT_DCDC_OTP_EN                 ( BIT(8) )
#define BITS_DCDC_OTP_S_RST(_x_)        ( (_x_) << 5 & (BIT(5)|BIT(6)|BIT(7)) )
#define BITS_DCDC_OTP_S(_x_)            ( (_x_) << 2 & (BIT(2)|BIT(3)|BIT(4)) )
#define BIT_DCDC_OTP_VBEOP_RST          ( BIT(1) )
#define BIT_DCDC_OTP_VBEOP              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_POR_SRC_STAT */
#define BIT_POR_PBCHGR_MASK_SET         ( BIT(15) )
#define BIT_EXT_RSTN_FLAG               ( BIT(10) )
#define BIT_CHGR_INT_FLAG               ( BIT(9) )
#define BIT_PB_INT2_FLAG                ( BIT(8) )
#define BIT_PB_INT_FLAG                 ( BIT(7) )
#define BIT_ALARM_INT_SET               ( BIT(6) )
#define BIT_CHGR_INT_1S_SET             ( BIT(5) )
#define BIT_CHGR_INT_DEBC               ( BIT(4) )
#define BIT_PB_INT2_1S_SET              ( BIT(3) )
#define BIT_PB_INT2_DEBC                ( BIT(2) )
#define BIT_PB_INT_1S_SET               ( BIT(1) )
#define BIT_PB_INT_DEBC                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_POR_SRC_FLAG_CLR */
#define BIT_EXT_RSTN_FLAG_RST           ( BIT(3) )
#define BIT_CHGR_INT_FLAG_RST           ( BIT(2) )
#define BIT_PBINT2_FLAG_RST             ( BIT(1) )
#define BIT_PBINT_FLAG_RST              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_HW_REBOOT_CTRL */
#define BIT_PBINT_HW_PD_EN              ( BIT(8) )
#define BIT_PBINT_6S_FLAG_CLR           ( BIT(7) )
#define BIT_PBINT_6S_FLAG               ( BIT(6) )
#define BITS_PBINT_HW_PD_THRESHOLD_RST(_x_)( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_PBINT_HW_PD_THRESHOLD_SET(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDCARM_CTRL0 */
#define BIT_DCDCARM_FRECUT_RST          ( BIT(15) )
#define BIT_DCDCARM_FRECUT              ( BIT(14) )
#define BIT_DCDCARM_PFM_RST             ( BIT(13) )
#define BIT_DCDCARM_PFM                 ( BIT(12) )
#define BIT_DCDCARM_DCM_RST             ( BIT(11) )
#define BIT_DCDCARM_DCM                 ( BIT(10) )
#define BIT_DCDCARM_DEDT_EN_RST         ( BIT(9) )
#define BIT_DCDCARM_DEDT_EN             ( BIT(8) )
#define BITS_DCDCARM_CTL_RST(_x_)       ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_DCDCARM_CTL(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDCARM_CTRL1 */
#define BITS_DCDCARM_PDRSLOW_RST(_x_)   ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DCDCARM_PDRSLOW(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_DCDCARM_CL_CTRL_RST         ( BIT(7) )
#define BIT_DCDCARM_CL_CTRL             ( BIT(6) )
#define BIT_DCDCARM_OSCSYCEN_SW         ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDCARM_CTRL2 */
#define BIT_DCDCARM_OSCSYCEN_HW_EN      ( BIT(14) )
#define BIT_DCDCARM_OSCSYC_DIV_EN       ( BIT(13) )
#define BITS_DCDCARM_OSCSYC_DIV(_x_)    ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_DCDCARM_RESERVER_RST(_x_)  ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_DCDCARM_RESERVER(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDCARM_CTRL_CAL */
#define BITS_DCDCARM_CAL_RST(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_DCDCARM_CAL(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDCMEM_CTRL0 */
#define BIT_DCDCMEM_FRECUT_RST          ( BIT(15) )
#define BIT_DCDCMEM_FRECUT              ( BIT(14) )
#define BIT_DCDCMEM_PFM_RST             ( BIT(13) )
#define BIT_DCDCMEM_PFM                 ( BIT(12) )
#define BIT_DCDCMEM_DCM_RST             ( BIT(11) )
#define BIT_DCDCMEM_DCM                 ( BIT(10) )
#define BIT_DCDCMEM_DEDT_EN_RST         ( BIT(9) )
#define BIT_DCDCMEM_DEDT_EN             ( BIT(8) )
#define BITS_DCDCMEM_CTL_40NM_RST(_x_)  ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DCDCMEM_CTL_40NM(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DCDCMEM_CTRL1 */
#define BITS_DCDCMEM_PDRSLOW_RST(_x_)   ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DCDCMEM_PDRSLOW(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_DCDCMEM_CL_CTRL_RST         ( BIT(7) )
#define BIT_DCDCMEM_CL_CTRL             ( BIT(6) )
#define BIT_DCDCMEM_OSCSYCEN_SW         ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDCMEM_CTRL2 */
#define BIT_DCDCMEM_OSCSYCEN_HW_EN      ( BIT(14) )
#define BIT_DCDCMEM_OSCSYC_DIV_EN       ( BIT(13) )
#define BITS_DCDCMEM_OSCSYC_DIV(_x_)    ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_DCDCMEM_RESERVER_RST(_x_)  ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_DCDCMEM_RESERVER(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDCMEM_CTRL_CAL */
#define BITS_DCDCMEM_CAL_RST(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_DCDCMEM_CAL(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DDR2_BUF_CTRL0_DS */
#define BITS_DDR2_BUF_CHNS_DS_RST(_x_)  ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DDR2_BUF_CHNS_DS(_x_)      ( (_x_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_DDR2_BUF_CHNS_RST(_x_)     ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_DDR2_BUF_CHNS(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_DDR2_BUF_S_DS_RST(_x_)     ( (_x_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_DDR2_BUF_S_DS(_x_)         ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DDR2_BUF_S_RST(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_DDR2_BUF_S(_x_)            ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DDR2_BUF_CTRL1_DS */
#define BIT_DDR2_BUF_PD_HW_RST          ( BIT(3) )
#define BIT_DDR2_BUF_PD_HW              ( BIT(2) )
#define BIT_DDR2_BUF_PD_RST             ( BIT(1) )
#define BIT_DDR2_BUF_PD                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_EFS_PROT */
/* write 16hC686 will set ana_efs_prot to 1; write other value will reset ana_efs_prot to 0.
 */
#define BITS_ANA_WFS_WD(_x_)            ( (_x_) << 1 & (BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ANA_EFS_PROT                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_EFS_CTRL */
#define BIT_EFS_2P5V_PWR_ON             ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDCLDO_CTRL0 */
#define BIT_DCDCLDO_FRECUT_RST          ( BIT(15) )
#define BIT_DCDCLDO_FRECUT              ( BIT(14) )
#define BIT_DCDCLDO_PFM_RST             ( BIT(13) )
#define BIT_DCDCLDO_PFM                 ( BIT(12) )
#define BIT_DCDCLDO_DCM_RST             ( BIT(11) )
#define BIT_DCDCLDO_DCM                 ( BIT(10) )
#define BIT_DCDCLDO_DEDT_EN_RST         ( BIT(9) )
#define BIT_DCDCLDO_DEDT_EN             ( BIT(8) )
#define BITS_DCDCLDO_CTL_RST(_x_)       ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_DCDCLDO_CTL(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDCLDO_CTRL1 */
#define BITS_DCDCLDO_PDRSLOW_RST(_x_)   ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DCDCLDO_PDRSLOW(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_DCDCLDO_CL_CTRL_RST         ( BIT(7) )
#define BIT_DCDCLDO_CL_CTRL             ( BIT(6) )
#define BIT_DCDCLDO_OSCSYCEN_SW         ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDCLDO_CTRL2 */
#define BIT_DCDCLDO_OSCSYCEN_HW_EN      ( BIT(14) )
#define BIT_DCDCLDO_OSCSYC_DIV_EN       ( BIT(13) )
#define BITS_DCDCLDO_OSCSYC_DIV(_x_)    ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_DCDCLDO_RESERVER_RST(_x_)  ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_DCDCLDO_RESERVER(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDCLDO_CTRL_CAL */
#define BITS_DCDCLDO_CAL_RST(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_DCDCLDO_CAL(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_AFUSE_CTRL */
/* prot key is 8ha2, it must write with rd_dly value
 */
#define BITS_AFUSE_RD_DLY_PROT(_x_)     ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
/* Software write 1 to this bit to issue an afuse read request,
 * and polling this bit, it will be cleared to 0 when afuse_out data successfully sampled
 * into AFUSE_OUT_LOW and HIGH register in the following.
 */
#define BIT_AFUSE_RD_REQ                ( BIT(7) )
#define BITS_AFUSE_RD_DLY(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

#define SHFT_AFUSE_RD_DLY               ( 0 )
#define MASK_AFUSE_RD_DLY               ( BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6) )

/* bits definitions for register ANA_REG_GLB_AFUSE_OUT_LOW */
#define BITS_AFUSE_OUT_LOW(_x_)         ( (_x_) << 0 )

/* bits definitions for register ANA_REG_GLB_AFUSE_OUT_HIGH */
#define BITS_AFUSE_OUT_HIGH(_x_)        ( (_x_) << 0 )

/* bits definitions for register ANA_REG_GLB_CHIP_ID_LOW */
#define BITS_CHIP_ID_LOW(_x_)           ( (_x_) << 0 )

/* bits definitions for register ANA_REG_GLB_CHIP_ID_HIGH */
#define BITS_CHIP_ID_HIGH(_x_)          ( (_x_) << 0 )

/* vars definitions for controller ANA_REGS_GLB */
#define KEY_EFS_PROT                    ( 0xc686UL )
#define KEY_RD_DLY                      ( 0xa2UL )

#endif //__ANA_REGS_GLB_H__
