/* the head file modifier:     g   2015-03-26 15:05:53*/

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
#error "Don't include this file directly, Pls include sci_glb_regs.h"
#endif


#ifndef __H_REGS_AP_AHB_HEADFILE_H__
#define __H_REGS_AP_AHB_HEADFILE_H__ __FILE__



#define REG_AP_AHB_AHB_EB                      SCI_ADDR(REGS_AP_AHB_BASE, 0x0000 )
#define REG_AP_AHB_AHB_RST                     SCI_ADDR(REGS_AP_AHB_BASE, 0x0004 )
#define REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG      SCI_ADDR(REGS_AP_AHB_BASE, 0x0008 )
#define REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG       SCI_ADDR(REGS_AP_AHB_BASE, 0x000C )
#define REG_AP_AHB_HOLDING_PEN                 SCI_ADDR(REGS_AP_AHB_BASE, 0x0010 )
#define REG_AP_AHB_MISC_CKG_EN                 SCI_ADDR(REGS_AP_AHB_BASE, 0x0014 )
#define REG_AP_AHB_MISC_CFG                    SCI_ADDR(REGS_AP_AHB_BASE, 0x0018 )
#define REG_AP_AHB_NANC_CLK_CFG                SCI_ADDR(REGS_AP_AHB_BASE, 0x3000 )
#define REG_AP_AHB_AP_QOS_CFG                  SCI_ADDR(REGS_AP_AHB_BASE, 0x3004 )
#define REG_AP_AHB_OTG_PHY_TUNE                SCI_ADDR(REGS_AP_AHB_BASE, 0x3008 )
#define REG_AP_AHB_OTG_PHY_TEST                SCI_ADDR(REGS_AP_AHB_BASE, 0x300C )
#define REG_AP_AHB_OTG_PHY_CTRL                SCI_ADDR(REGS_AP_AHB_BASE, 0x3010 )
#define REG_AP_AHB_HSIC_PHY_TUNE               SCI_ADDR(REGS_AP_AHB_BASE, 0x3014 )
#define REG_AP_AHB_HSIC_PHY_TEST               SCI_ADDR(REGS_AP_AHB_BASE, 0x3018 )
#define REG_AP_AHB_HSIC_PHY_CTRL               SCI_ADDR(REGS_AP_AHB_BASE, 0x301C )
#define REG_AP_AHB_USB3_CTRL                   SCI_ADDR(REGS_AP_AHB_BASE, 0x3020 )
#define REG_AP_AHB_MISC_CTRL                   SCI_ADDR(REGS_AP_AHB_BASE, 0x3024 )
#define REG_AP_AHB_MISC_STATUS                 SCI_ADDR(REGS_AP_AHB_BASE, 0x3028 )
#define REG_AP_AHB_NOC_CTRL0                   SCI_ADDR(REGS_AP_AHB_BASE, 0x302C )
#define REG_AP_AHB_NOC_CTRL1                   SCI_ADDR(REGS_AP_AHB_BASE, 0x3030 )
#define REG_AP_AHB_NOC_CTRL2                   SCI_ADDR(REGS_AP_AHB_BASE, 0x3034 )
#define REG_AP_AHB_NOC_CTRL3                   SCI_ADDR(REGS_AP_AHB_BASE, 0x3038 )
#define REG_AP_AHB_USB3_DBG0                   SCI_ADDR(REGS_AP_AHB_BASE, 0x303C )
#define REG_AP_AHB_USB3_DBG1                   SCI_ADDR(REGS_AP_AHB_BASE, 0x3040 )
#define REG_AP_AHB_USB3_DBG2                   SCI_ADDR(REGS_AP_AHB_BASE, 0x3044 )

/* bits definitions for register REG_AP_AHB_AHB_EB */
#define BIT_AP_AHB_CC63P_EB                                     BIT(23)
#define BIT_AP_AHB_CC63S_EB                                     BIT(22)
#define BIT_AP_AHB_ZIPMTX_EB                                    BIT(21)
#define BIT_AP_AHB_ZIPDEC_EB                                    BIT(20)
#define BIT_AP_AHB_ZIPENC_EB                                    BIT(19)
#define BIT_AP_AHB_NANDC_ECC_EB                                 BIT(18)
#define BIT_AP_AHB_NANDC_2X_EB                                  BIT(17)
#define BIT_AP_AHB_NANDC_EB                                     BIT(16)
#define BIT_AP_AHB_BUSMON_EB                                    BIT(13)
#define BIT_AP_AHB_ROM_EB                                       BIT(12)
#define BIT_AP_AHB_EMMC_EB                                      BIT(10)
#define BIT_AP_AHB_SDIO2_EB                                     BIT(9)
#define BIT_AP_AHB_SDIO1_EB                                     BIT(8)
#define BIT_AP_AHB_SDIO0_EB                                     BIT(7)
#define BIT_AP_AHB_NFC_EB                                       BIT(6)
#define BIT_AP_AHB_DMA_EB                                       BIT(5)
#define BIT_DMA_EB                                              BIT_AP_AHB_DMA_EB
#define BIT_AP_AHB_USB3_REF_EB                                  BIT(4)
#define BIT_AP_AHB_USB3_SUSPEND_EB                              BIT(3)
#define BIT_AP_AHB_USB3_EB                                      BIT(2)
#define BIT_AP_AHB_OTG_EB                                       BIT(1)
#define BIT_USB_EB                                              BIT_AP_AHB_OTG_EB
#define BIT_AP_AHB_HSIC_EB                                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_AHB_RST
// Register Offset : 0x0004
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_CC63P_SOFT_RST                               BIT(31)
#define BIT_AP_AHB_CC63S_SOFT_RST                               BIT(30)
#define BIT_AP_AHB_HSIC_PHY_SOFT_RST                            BIT(29)
#define BIT_AP_AHB_HSIC_UTMI_SOFT_RST                           BIT(28)
#define BIT_AP_AHB_HSIC_SOFT_RST                                BIT(27)
#define BIT_AP_AHB_ZIP_MTX_SOFT_RST                             BIT(21)
#define BIT_AP_AHB_ZIPDEC_SOFT_RST                              BIT(20)
#define BIT_AP_AHB_ZIPENC_SOFT_RST                              BIT(19)
#define BIT_AP_AHB_BUSMON_SOFT_RST                              BIT(14)
#define BIT_AP_AHB_SPINLOCK_SOFT_RST                            BIT(13)
#define BIT_AP_AHB_EMMC_SOFT_RST                                BIT(11)
#define BIT_AP_AHB_SDIO2_SOFT_RST                               BIT(10)
#define BIT_AP_AHB_SDIO1_SOFT_RST                               BIT(9)
#define BIT_AP_AHB_SDIO0_SOFT_RST                               BIT(8)
#define BIT_AP_AHB_DRM_SOFT_RST                                 BIT(7)
#define BIT_AP_AHB_NFC_SOFT_RST                                 BIT(6)
#define BIT_AP_AHB_DMA_SOFT_RST                                 BIT(5)
#define BIT_DMA_SOFT_RST                                        BIT_AP_AHB_DMA_SOFT_RST
#define BIT_AP_AHB_OTG_PHY_SOFT_RST                             BIT(3)
#define BIT_OTG_PHY_SOFT_RST                                    BIT_AP_AHB_OTG_PHY_SOFT_RST
#define BIT_AP_AHB_OTG_UTMI_SOFT_RST                            BIT(2)
#define BIT_OTG_UTMI_SOFT_RST                                   BIT_AP_AHB_OTG_UTMI_SOFT_RST
#define BIT_AP_AHB_USB3_SOFT_RST                                BIT(1)
#define BIT_AP_AHB_OTG_SOFT_RST                                 BIT(0)
#define BIT_OTG_SOFT_RST                                        BIT_AP_AHB_OTG_SOFT_RST

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG
// Register Offset : 0x0008
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_AP_PERI_FORCE_ON                             BIT(2)
#define BIT_AP_AHB_AP_PERI_FORCE_SLP                            BIT(1)
#define BIT_AP_AHB_AP_APB_SLEEP                                 BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG
// Register Offset : 0x000C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_DMA_ACT_LIGHT_EN                             BIT(2)
#define BIT_AP_AHB_AP_AHB_AUTO_GATE_EN                          BIT(1)
#define BIT_AP_AHB_AP_EMC_AUTO_GATE_EN                          BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_HOLDING_PEN
// Register Offset : 0x0010
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_HOLDING_PEN(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_MISC_CKG_EN
// Register Offset : 0x0014
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_BUS_CLK_FORCE_ON(x)                          (((x) & 0x3FFF) << 2)
#define BIT_AP_AHB_DPHY_REF_CKG_EN                              BIT(1)
#define BIT_AP_AHB_DPHY_CFG_CKG_EN                              BIT(0)
#define BIT_DPHY_REF_CKG_EN                                     (BIT_AP_AHB_DPHY_REF_CKG_EN)
#define BIT_DPHY_CFG_CKG_EN					                 	(BIT_AP_AHB_DPHY_CFG_CKG_EN)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_MISC_CFG
// Register Offset : 0x0018
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_AP_R_RES(x)                                  (((x) & 0x1F) << 27)
#define BIT_AP_AHB_OTG_HADDR_EXT(x)                             (((x) & 0x7) << 24)
#define BIT_AP_AHB_HSIC_HADDR_EXT(x)                            (((x) & 0x7) << 21)
#define BIT_AP_AHB_SDIO2_SLV_SEL                                BIT(20)
#define BIT_AP_AHB_EMMC_SLOT_SEL(x)                             (((x) & 0x3) << 18)
#define BIT_AP_AHB_SDIO0_SLOT_SEL(x)                            (((x) & 0x3) << 16)
#define BIT_AP_AHB_AP_RW_RES(x)                                 (((x) & 0x3F) << 10)
#define BIT_AP_AHB_M0_NIU_IDLE_DET_DIS                          BIT(9)
#define BIT_AP_AHB_BUS_VALID_CNT(x)                             (((x) & 0x1F) << 4)
#define BIT_AP_AHB_SDIO2_SLOT_SEL(x)                            (((x) & 0x3) << 2)
#define BIT_AP_AHB_SDIO1_SLOT_SEL(x)                            (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_NANC_CLK_CFG
// Register Offset : 0x3000
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_CLK_NANDC2X_DIV(x)                           (((x) & 0x3) << 2)
#define BIT_AP_AHB_CLK_NANDC2X_SEL(x)                           (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_AP_QOS_CFG
// Register Offset : 0x3004
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_AP_QOS_ID7(x)                                (((x) & 0xF) << 28)
#define BIT_AP_AHB_AP_QOS_ID6(x)                                (((x) & 0xF) << 24)
#define BIT_AP_AHB_AP_QOS_ID5(x)                                (((x) & 0xF) << 20)
#define BIT_AP_AHB_AP_QOS_ID4(x)                                (((x) & 0xF) << 16)
#define BIT_AP_AHB_AP_QOS_ID3(x)                                (((x) & 0xF) << 12)
#define BIT_AP_AHB_AP_QOS_ID2(x)                                (((x) & 0xF) << 8)
#define BIT_AP_AHB_AP_QOS_ID1(x)                                (((x) & 0xF) << 4)
#define BIT_AP_AHB_AP_QOS_ID0(x)                                (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_OTG_PHY_TUNE
// Register Offset : 0x3008
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_OTG_TXPREEMPPULSETUNE                        BIT(20)
#define BIT_AP_AHB_OTG_TXRESTUNE(x)                             (((x) & 0x3) << 18)
#define BIT_AP_AHB_OTG_TXHSXVTUNE(x)                            (((x) & 0x3) << 16)
#define BIT_AP_AHB_OTG_TXVREFTUNE(x)                            (((x) & 0xF) << 12)
#define BIT_AP_AHB_OTG_TXPREEMPAMPTUNE(x)                       (((x) & 0x3) << 10)
#define BIT_AP_AHB_OTG_TXRISETUNE(x)                            (((x) & 0x3) << 8)
#define BIT_AP_AHB_OTG_TXFSLSTUNE(x)                            (((x) & 0xF) << 4)
#define BIT_AP_AHB_OTG_SQRXTUNE(x)                              (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_OTG_PHY_TEST
// Register Offset : 0x300C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_OTG_ATERESET                                 BIT(31)
#define BIT_AP_AHB_OTG_VBUS_VALID_EXT_SEL                       BIT(26)
#define BIT_AP_AHB_OTG_VBUS_VALID_EXT                           BIT(25)
#define BIT_AP_AHB_OTG_OTGDISABLE                               BIT(24)
#define BIT_AP_AHB_OTG_TESTBURNIN                               BIT(21)
#define BIT_AP_AHB_OTG_LOOPBACKENB                              BIT(20)
#define BIT_AP_AHB_OTG_TESTDATAOUT(x)                           (((x) & 0xF) << 16)
#define BIT_AP_AHB_OTG_VATESTENB(x)                             (((x) & 0x3) << 14)
#define BIT_AP_AHB_OTG_TESTCLK                                  BIT(13)
#define BIT_AP_AHB_OTG_TESTDATAOUTSEL                           BIT(12)
#define BIT_AP_AHB_OTG_TESTADDR(x)                              (((x) & 0xF) << 8)
#define BIT_AP_AHB_OTG_TESTDATAIN(x)                            (((x) & 0xFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_OTG_PHY_CTRL
// Register Offset : 0x3010
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_OTG_SS_SCALEDOWNMODE(x)                      (((x) & 0x3) << 25)
#define BIT_AP_AHB_OTG_TXBITSTUFFENH                            BIT(23)
#define BIT_AP_AHB_OTG_TXBITSTUFFEN                             BIT(22)
#define BIT_AP_AHB_OTG_DMPULLDOWN                               BIT(21)
#define BIT_AP_AHB_OTG_DPPULLDOWN                               BIT(20)
#define BIT_AP_AHB_OTG_DMPULLUP                                 BIT(9)
#define BIT_AP_AHB_OTG_COMMONONN                                BIT(8)
#define BIT_AP_AHB_OTG_REFCLKSEL(x)                             (((x) & 0x3) << 4)
#define BIT_AP_AHB_OTG_FSEL(x)                                  (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_HSIC_PHY_TUNE
// Register Offset : 0x3014
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_HSIC_REFCLK_DIV(x)                           (((x) & 0x7F) << 24)
#define BIT_AP_AHB_HSIC_TXPREEMPPULSETUNE                       BIT(20)
#define BIT_AP_AHB_HSIC_TXRESTUNE(x)                            (((x) & 0x3) << 18)
#define BIT_AP_AHB_HSIC_TXHSXVTUNE(x)                           (((x) & 0x3) << 16)
#define BIT_AP_AHB_HSIC_TXVREFTUNE(x)                           (((x) & 0xF) << 12)
#define BIT_AP_AHB_HSIC_TXPREEMPAMPTUNE(x)                      (((x) & 0x3) << 10)
#define BIT_AP_AHB_HSIC_TXRISETUNE(x)                           (((x) & 0x3) << 8)
#define BIT_AP_AHB_HSIC_TXFSLSTUNE(x)                           (((x) & 0xF) << 4)
#define BIT_AP_AHB_HSIC_SQRXTUNE(x)                             (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_HSIC_PHY_TEST
// Register Offset : 0x3018
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_HSIC_ATERESET                                BIT(31)
#define BIT_AP_AHB_HSIC_VBUS_VALID_EXT_SEL                      BIT(26)
#define BIT_AP_AHB_HSIC_VBUS_VALID_EXT                          BIT(25)
#define BIT_AP_AHB_HSIC_OTGDISABLE                              BIT(24)
#define BIT_AP_AHB_HSIC_TESTBURNIN                              BIT(21)
#define BIT_AP_AHB_HSIC_LOOPBACKENB                             BIT(20)
#define BIT_AP_AHB_HSIC_TESTDATAOUT(x)                          (((x) & 0xF) << 16)
#define BIT_AP_AHB_HSIC_VATESTENB(x)                            (((x) & 0x3) << 14)
#define BIT_AP_AHB_HSIC_TESTCLK                                 BIT(13)
#define BIT_AP_AHB_HSIC_TESTDATAOUTSEL                          BIT(12)
#define BIT_AP_AHB_HSIC_TESTADDR(x)                             (((x) & 0xF) << 8)
#define BIT_AP_AHB_HSIC_TESTDATAIN(x)                           (((x) & 0xFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_HSIC_PHY_CTRL
// Register Offset : 0x301C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_HSIC_SS_SCALEDOWNMODE(x)                     (((x) & 0x3) << 25)
#define BIT_AP_AHB_HSIC_TXBITSTUFFENH                           BIT(23)
#define BIT_AP_AHB_HSIC_TXBITSTUFFEN                            BIT(22)
#define BIT_AP_AHB_HSIC_DMPULLDOWN                              BIT(21)
#define BIT_AP_AHB_HSIC_DPPULLDOWN                              BIT(20)
#define BIT_AP_AHB_HSIC_IF_MODE                                 BIT(16)
#define BIT_AP_AHB_IF_SELECT_HSIC                               BIT(13)
#define BIT_AP_AHB_HSIC_DBNCE_FLTR_BYPASS                       BIT(12)
#define BIT_AP_AHB_HSIC_DMPULLUP                                BIT(9)
#define BIT_AP_AHB_HSIC_COMMONONN                               BIT(8)
#define BIT_AP_AHB_HSIC_REFCLKSEL(x)                            (((x) & 0x3) << 4)
#define BIT_AP_AHB_HSIC_FSEL(x)                                 (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_USB3_CTRL
// Register Offset : 0x3020
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_HOST_SYSTEM_ERR                              BIT(12)
#define BIT_AP_AHB_HOST_PORT_POWER_CONTROL_PRESENT              BIT(11)
#define BIT_AP_AHB_FLADJ_30MHZ_REG(x)                           (((x) & 0x3F) << 5)
#define BIT_AP_AHB_PME_EN                                       BIT(4)
#define BIT_AP_AHB_BUS_FILTER_BYPASS(x)                         (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_MISC_CTRL
// Register Offset : 0x3024
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_MAINHURRY_HSIC(x)                            (((x) & 0x7) << 27)
#define BIT_AP_AHB_MAINPRESS_HSIC(x)                            (((x) & 0x7) << 24)
#define BIT_AP_AHB_MAINHURRY_OTG(x)                             (((x) & 0x7) << 21)
#define BIT_AP_AHB_MAINPRESS_OTG(x)                             (((x) & 0x7) << 18)
#define BIT_AP_AHB_CC63P_APBS_PPROT(x)                          (((x) & 0x7) << 15)
#define BIT_AP_AHB_CC63S_APBS_PPROT(x)                          (((x) & 0x7) << 12)
#define BIT_AP_AHB_AWPROT_ZIPDEC(x)                             (((x) & 0x7) << 9)
#define BIT_AP_AHB_ARPROT_ZIPDEC(x)                             (((x) & 0x7) << 6)
#define BIT_AP_AHB_AWPROT_ZIPENC(x)                             (((x) & 0x7) << 3)
#define BIT_AP_AHB_ARPROT_ZIPENC(x)                             (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_MISC_STATUS
// Register Offset : 0x3028
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_INT_REQ_CC63P                                BIT(1)
#define BIT_AP_AHB_INT_REQ_CC63S                                BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_NOC_CTRL0
// Register Offset : 0x302C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_M9_QOS(x)                                    (((x) & 0xF) << 28)
#define BIT_AP_AHB_M7_QOS(x)                                    (((x) & 0xF) << 24)
#define BIT_AP_AHB_M6_QOS(x)                                    (((x) & 0xF) << 20)
#define BIT_AP_AHB_M5_QOS(x)                                    (((x) & 0xF) << 16)
#define BIT_AP_AHB_M4_QOS(x)                                    (((x) & 0xF) << 12)
#define BIT_AP_AHB_M3_QOS(x)                                    (((x) & 0xF) << 8)
#define BIT_AP_AHB_M2_QOS(x)                                    (((x) & 0xF) << 4)
#define BIT_AP_AHB_M1_QOS(x)                                    (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_NOC_CTRL1
// Register Offset : 0x3030
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_S5_T_MAINEXTEN                               BIT(21)
#define BIT_AP_AHB_S4_T_MAINEXTEN                               BIT(20)
#define BIT_AP_AHB_S3_T_MAINEXTEN                               BIT(19)
#define BIT_AP_AHB_S2_T_MAINEXTEN                               BIT(18)
#define BIT_AP_AHB_S1_T_MAINEXTEN                               BIT(17)
#define BIT_AP_AHB_S0_T_MAINEXTEN                               BIT(16)
#define BIT_AP_AHB_M14_QOS(x)                                   (((x) & 0xF) << 12)
#define BIT_AP_AHB_M13_QOS(x)                                   (((x) & 0xF) << 8)
#define BIT_AP_AHB_M12_QOS(x)                                   (((x) & 0xF) << 4)
#define BIT_AP_AHB_M11_QOS(x)                                   (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_NOC_CTRL2
// Register Offset : 0x3034
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_M12_ARCACHE(x)                               (((x) & 0xF) << 28)
#define BIT_AP_AHB_M11_ARCACHE(x)                               (((x) & 0xF) << 24)
#define BIT_AP_AHB_M7_ARCACHE(x)                                (((x) & 0xF) << 20)
#define BIT_AP_AHB_M6_ARCACHE(x)                                (((x) & 0xF) << 16)
#define BIT_AP_AHB_M5_ARCACHE(x)                                (((x) & 0xF) << 12)
#define BIT_AP_AHB_M4_ARCACHE(x)                                (((x) & 0xF) << 8)
#define BIT_AP_AHB_M3_ARCACHE(x)                                (((x) & 0xF) << 4)
#define BIT_AP_AHB_M2_ARCACHE(x)                                (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_NOC_CTRL3
// Register Offset : 0x3038
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_M12_AWCACHE(x)                               (((x) & 0xF) << 28)
#define BIT_AP_AHB_M11_AWCACHE(x)                               (((x) & 0xF) << 24)
#define BIT_AP_AHB_M7_AWCACHE(x)                                (((x) & 0xF) << 20)
#define BIT_AP_AHB_M6_AWCACHE(x)                                (((x) & 0xF) << 16)
#define BIT_AP_AHB_M5_AWCACHE(x)                                (((x) & 0xF) << 12)
#define BIT_AP_AHB_M4_AWCACHE(x)                                (((x) & 0xF) << 8)
#define BIT_AP_AHB_M3_AWCACHE(x)                                (((x) & 0xF) << 4)
#define BIT_AP_AHB_M2_AWCACHE(x)                                (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_USB3_DBG0
// Register Offset : 0x303C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_USB3_DUG_0(x)                                (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_USB3_DBG1
// Register Offset : 0x3040
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_USB3_DUG_1(x)                                (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_AHB_USB3_DBG2
// Register Offset : 0x3044
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_AHB_USB3_DUG_2(x)                                (((x) & 0x3))

#endif
