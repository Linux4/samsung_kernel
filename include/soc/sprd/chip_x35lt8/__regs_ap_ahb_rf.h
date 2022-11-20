/* the head file modifier:     g   2015-03-19 15:39:58*/

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


#ifndef __H_REGS_AP_AHB_RF_HEADFILE_H__
#define __H_REGS_AP_AHB_RF_HEADFILE_H__ __FILE__

#define  REGS_AP_AHB_RF

/* registers definitions for AP_AHB_RF */
#define REG_AP_AHB_AHB_EB				SCI_ADDR(REGS_AP_AHB_BASE, 0x0000)/*AHB_EB*/
#define REG_AP_AHB_AHB_RST				SCI_ADDR(REGS_AP_AHB_BASE, 0x0004)/*AHB_RST*/
#define REG_AP_AHB_APCPU_RST_SET			SCI_ADDR(REGS_AP_AHB_BASE, 0x0008)/*APCPU_RST_SET*/
#define REG_AP_AHB_CA7_RST_SET          REG_AP_AHB_APCPU_RST_SET
#define REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG		SCI_ADDR(REGS_AP_AHB_BASE, 0x000C)/*AP_SYS_FORCE_SLEEP_CFG*/
#define REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG		SCI_ADDR(REGS_AP_AHB_BASE, 0x0010)/*AP_SYS_AUTO_SLEEP_CFG*/
#define REG_AP_AHB_HOLDING_PEN			SCI_ADDR(REGS_AP_AHB_BASE, 0x0014)/*HOLDING_PEN*/
#define REG_AP_AHB_JMP_ADDR_APCPU_C0			SCI_ADDR(REGS_AP_AHB_BASE, 0x0018)/*JMP_ADDR_APCPU_C0*/
#define REG_AP_AHB_JMP_ADDR_APCPU_C1			SCI_ADDR(REGS_AP_AHB_BASE, 0x001C)/*JMP_ADDR_APCPU_C1*/
#define REG_AP_AHB_JMP_ADDR_APCPU_C2			SCI_ADDR(REGS_AP_AHB_BASE, 0x0020)/*JMP_ADDR_APCPU_C2*/
#define REG_AP_AHB_JMP_ADDR_APCPU_C3			SCI_ADDR(REGS_AP_AHB_BASE, 0x0024)/*JMP_ADDR_APCPU_C3*/
#define REG_AP_AHB_APCPU_C0_PU_LOCK			SCI_ADDR(REGS_AP_AHB_BASE, 0x0028)/*APCPU_C0_PU_LOCK*/
#define REG_AP_AHB_APCPU_C1_PU_LOCK			SCI_ADDR(REGS_AP_AHB_BASE, 0x002C)/*APCPU_C1_PU_LOCK*/
#define REG_AP_AHB_APCPU_C2_PU_LOCK			SCI_ADDR(REGS_AP_AHB_BASE, 0x0030)/*APCPU_C2_PU_LOCK*/
#define REG_AP_AHB_APCPU_C3_PU_LOCK			SCI_ADDR(REGS_AP_AHB_BASE, 0x0034)/*APCPU_C3_PU_LOCK*/
#define REG_AP_AHB_APCPU_CKG_DIV_CFG			SCI_ADDR(REGS_AP_AHB_BASE, 0x0038)/*APCPU_CKG_DIV_CFG*/
#define REG_AP_AHB_JMP_ADDR_CA7_C0      REG_AP_AHB_JMP_ADDR_APCPU_C0
#define REG_AP_AHB_JMP_ADDR_CA7_C1      REG_AP_AHB_JMP_ADDR_APCPU_C1
#define REG_AP_AHB_JMP_ADDR_CA7_C2      REG_AP_AHB_JMP_ADDR_APCPU_C2
#define REG_AP_AHB_JMP_ADDR_CA7_C3      REG_AP_AHB_JMP_ADDR_APCPU_C3
#define REG_AP_AHB_CA7_C0_PU_LOCK       REG_AP_AHB_APCPU_C0_PU_LOCK	
#define REG_AP_AHB_CA7_C1_PU_LOCK       REG_AP_AHB_APCPU_C1_PU_LOCK
#define REG_AP_AHB_CA7_C2_PU_LOCK       REG_AP_AHB_APCPU_C2_PU_LOCK
#define REG_AP_AHB_CA7_C3_PU_LOCK       REG_AP_AHB_APCPU_C3_PU_LOCK
#define REG_AP_AHB_CA7_CKG_DIV_CFG      REG_AP_AHB_APCPU_CKG_DIV_CFG
#define REG_AP_AHB_MCU_PAUSE				SCI_ADDR(REGS_AP_AHB_BASE, 0x003C)/*MCU_PAUSE*/
#define REG_AP_AHB_MISC_CKG_EN			SCI_ADDR(REGS_AP_AHB_BASE, 0x0040)/*MISC_CKG_EN*/
#define REG_AP_AHB_APCPU_C0_AUTO_FORCE_SHUTDOWN_EN	SCI_ADDR(REGS_AP_AHB_BASE, 0x0044)/*APCPU_C0_AUTO_FORCE_SHUTDOWN_EN*/
#define REG_AP_AHB_APCPU_C1_AUTO_FORCE_SHUTDOWN_EN	SCI_ADDR(REGS_AP_AHB_BASE, 0x0048)/*APCPU_C1_AUTO_FORCE_SHUTDOWN_EN*/
#define REG_AP_AHB_APCPU_C2_AUTO_FORCE_SHUTDOWN_EN	SCI_ADDR(REGS_AP_AHB_BASE, 0x004C)/*APCPU_C2_AUTO_FORCE_SHUTDOWN_EN*/
#define REG_AP_AHB_APCPU_C3_AUTO_FORCE_SHUTDOWN_EN	SCI_ADDR(REGS_AP_AHB_BASE, 0x0050)/*APCPU_C3_AUTO_FORCE_SHUTDOWN_EN*/
#define REG_AP_AHB_APCPU_CKG_SEL_CFG			SCI_ADDR(REGS_AP_AHB_BASE, 0x0054)/*APCPU_CKG_SEL_CFG*/
#define REG_AP_AHB_APCPU_AUTO_GATE_EN		SCI_ADDR(REGS_AP_AHB_BASE, 0x0058)/*APCPU_AUTO_GATE_EN*/
#define REG_AP_AHB_CA7_C0_AUTO_FORCE_SHUTDOWN_EN REG_AP_AHB_APCPU_C0_AUTO_FORCE_SHUTDOWN_EN
#define REG_AP_AHB_CA7_C1_AUTO_FORCE_SHUTDOWN_EN REG_AP_AHB_APCPU_C1_AUTO_FORCE_SHUTDOWN_EN
#define REG_AP_AHB_CA7_C2_AUTO_FORCE_SHUTDOWN_EN REG_AP_AHB_APCPU_C2_AUTO_FORCE_SHUTDOWN_EN
#define REG_AP_AHB_CA7_C3_AUTO_FORCE_SHUTDOWN_EN REG_AP_AHB_APCPU_C3_AUTO_FORCE_SHUTDOWN_EN
#define REG_AP_AHB_CA7_CKG_SEL_CFG      REG_AP_AHB_APCPU_CKG_SEL_CFG
#define REG_AP_AHB_APCPU_GIC_CKG_CFG			SCI_ADDR(REGS_AP_AHB_BASE, 0x005c)/*APCPU_GIC_CKG_CFG*/
#define REG_AP_AHB_APCPU_GIC_CLK_CFG			SCI_ADDR(REGS_AP_AHB_BASE, 0x0060)/*APCPU_GIC_CLK_CFG*/
#define REG_AP_AHB_APCPU_LIT_CKG_EN			SCI_ADDR(REGS_AP_AHB_BASE, 0x0064)/*APCPU_LIT_CKG_EN*/
#define REG_AP_AHB_APCPU_BIG_CKG_EN			SCI_ADDR(REGS_AP_AHB_BASE, 0x0068)/*APCPU_LIT_CKG_EN*/
#define REG_AP_AHB_APCPU_LIT_L2FLUSH			SCI_ADDR(REGS_AP_AHB_BASE, 0x006C)/*APCPU_LIT_L2FLUSH*/
#define REG_AP_AHB_APCPU_BIG_L2FLUSH			SCI_ADDR(REGS_AP_AHB_BASE, 0x0070)/*APCPU_BIG_L2FLUSH*/
#define REG_AP_AHB_APCPU_CCI_BASE_H25		SCI_ADDR(REGS_AP_AHB_BASE, 0x0074)/*APCPU_CCI_BASE_H25*/
#define REG_AP_AHB_APCPU_CCI_CTRL			SCI_ADDR(REGS_AP_AHB_BASE, 0x0078)/*APCPU_CCI_CTRL*/
#define REG_AP_AHB_MISC_CFG				SCI_ADDR(REGS_AP_AHB_BASE, 0x3000)/*MISC_CFG*/
#define REG_AP_AHB_AP_MAIN_MTX_HPROT_CFG		SCI_ADDR(REGS_AP_AHB_BASE, 0x3004)/*AP_MAIN_MTX_HPROT_CFG*/
#define REG_AP_AHB_APCPU_STANDBY_STATUS		SCI_ADDR(REGS_AP_AHB_BASE, 0x3008)/*APCPU_STANDBY_STATUS*/
#define REG_AP_AHB_CA7_STANDBY_STATUS	REG_AP_AHB_APCPU_STANDBY_STATUS
#define REG_AP_AHB_NANC_CLK_CFG			SCI_ADDR(REGS_AP_AHB_BASE, 0x300C)/*NANDC_CLK_CFG*/
#define REG_AP_AHB_LVDS_CFG				SCI_ADDR(REGS_AP_AHB_BASE, 0x3010)/*LVDS_CFG*/
#define REG_AP_AHB_LVDS_PLL_CFG0			SCI_ADDR(REGS_AP_AHB_BASE, 0x3014)/*LVDS_PLL_CFG0*/
#define REG_AP_AHB_LVDS_PLL_CFG1			SCI_ADDR(REGS_AP_AHB_BASE, 0x3018)/*LVDS_PLL_CFG1*/
#define REG_AP_AHB_AP_QOS_CFG			SCI_ADDR(REGS_AP_AHB_BASE, 0x301C)/*AP_QOS_CFG*/
#define REG_AP_AHB_OTG_PHY_TUNE			SCI_ADDR(REGS_AP_AHB_BASE, 0x3020)/*USB_PHY_TUNE*/
#define REG_AP_AHB_OTG_PHY_TEST			SCI_ADDR(REGS_AP_AHB_BASE, 0x3024)/*MISC_CKG_EN*/
#define REG_AP_AHB_OTG_PHY_CTRL			SCI_ADDR(REGS_AP_AHB_BASE, 0x3028)/*USB_PHY_CTRL*/
#define REG_AP_AHB_HSIC_PHY_TUNE			SCI_ADDR(REGS_AP_AHB_BASE, 0x302C)/*USB_PHY_TUNE*/
#define REG_AP_AHB_HSIC_PHY_TEST			SCI_ADDR(REGS_AP_AHB_BASE, 0x3030)/*MISC_CKG_EN*/
#define REG_AP_AHB_HSIC_PHY_CTRL			SCI_ADDR(REGS_AP_AHB_BASE, 0x3034)/*USB_PHY_CTRL*/
#define REG_AP_AHB_ZIP_MTX_QOS_CFG			SCI_ADDR(REGS_AP_AHB_BASE, 0x3038)/*ZIP_MTX_QOS_CFG*/
#define REG_AP_AHB_APCPU_QCHN_CTL_BIG		SCI_ADDR(REGS_AP_AHB_BASE, 0x303C)/*APCPU_QCHN_CTL_BIG*/
#define REG_AP_AHB_APCPU_QCHN_STA_BIG		SCI_ADDR(REGS_AP_AHB_BASE, 0x3040)/*APCPU_QCHN_STA_BIG*/
#define REG_AP_AHB_APCPU_QCHN_CTL_LIT		SCI_ADDR(REGS_AP_AHB_BASE, 0x3044)/*APCPU_QCHN_CTL_LIT*/
#define REG_AP_AHB_APCPU_QCHN_STA_LIT		SCI_ADDR(REGS_AP_AHB_BASE, 0x3048)/*APCPU_QCHN_STA_LIT*/
#define REG_AP_AHB_GSP_SEL				SCI_ADDR(REGS_AP_AHB_BASE, 0x304c)/*GSP_SEL*/
#define REG_AP_AHB_RES_REG				SCI_ADDR(REGS_AP_AHB_BASE, 0x30F0)/*RES_REG*/
#define REG_AP_AHB_CHIP_ID				SCI_ADDR(REGS_AP_AHB_BASE, 0x30FC)/*CHIP_ID*/



/* bits definitions for register REG_AP_AHB_AHB_EB */
#define BIT_ZIPMTX_EB						( BIT(23) )
#define BIT_LVDS_EB						( BIT(22) )
#define BIT_ZIPDEC_EB						( BIT(21) )
#define BIT_ZIPENC_EB						( BIT(20) )
#define BIT_NANDC_ECC_EB					( BIT(19) )
#define BIT_NANDC_2X_EB						( BIT(18) )
#define BIT_NANDC_EB						( BIT(17) )
#define BIT_BUSMON2_EB						( BIT(16) )
#define BIT_BUSMON1_EB						( BIT(15) )
#define BIT_BUSMON0_EB						( BIT(14) )
#define BIT_SPINLOCK_EB						( BIT(13) )
#define BIT_EMMC_EB						( BIT(11) )
#define BIT_SDIO2_EB						( BIT(10) )
#define BIT_SDIO1_EB						( BIT(9) )
#define BIT_SDIO0_EB						( BIT(8) )
#define BIT_DRM_EB						( BIT(7) )
#define BIT_NFC_EB						( BIT(6) )
#define BIT_DMA_EB						( BIT(5) )
#define BIT_OTG_EB						( BIT(4) )
#define BIT_USB_EB						(BIT_OTG_EB)
#define BIT_GSP_EB						( BIT(3) )
#define BIT_HSIC_EB						( BIT(2) )
#define BIT_DISPC_EB						( BIT(1) )
#define BIT_DISPC0_EB					(BIT_DISPC_EB)
#define BIT_DSI_EB						( BIT(0) )

/* bits definitions for register REG_AP_AHB_AHB_RST */
#define BIT_HSIC_PHY_SOFT_RST					( BIT(30) )
#define BIT_HSIC_UTMI_SOFT_RST					( BIT(29) )
#define BIT_HSIC_SOFT_RST					( BIT(28) )
#define BIT_LVDS_SOFT_RST					( BIT(25) )
#define BIT_ZIP_MTX_SOFT_RST					( BIT(24) )
#define BIT_ZIPDEC_SOFT_RST					( BIT(23) )
#define BIT_ZIPENC_SOFT_RST					( BIT(22) )
#define BIT_CCI_SOFT_RST					( BIT(21) )
#define BIT_NANDC_SOFT_RST					( BIT(20) )
#define BIT_BUSMON2_SOFT_RST					( BIT(19) )
#define BIT_BUSMON1_SOFT_RST					( BIT(18) )
#define BIT_BUSMON0_SOFT_RST					( BIT(17) )
#define BIT_SPINLOCK_SOFT_RST					( BIT(16) )
#define BIT_EMMC_SOFT_RST					( BIT(14) )
#define BIT_SDIO2_SOFT_RST					( BIT(13) )
#define BIT_SDIO1_SOFT_RST					( BIT(12) )
#define BIT_SDIO0_SOFT_RST					( BIT(11) )
#define BIT_DRM_SOFT_RST					( BIT(10) )
#define BIT_NFC_SOFT_RST					( BIT(9) )
#define BIT_DMA_SOFT_RST					( BIT(8) )
#define BIT_CSSYS_SOFT_RST					( BIT(7) )
#define BIT_OTG_PHY_SOFT_RST					( BIT(6) )
#define BIT_OTG_UTMI_SOFT_RST					( BIT(5) )
#define BIT_OTG_SOFT_RST					( BIT(4) )
#define BIT_GSP_SOFT_RST					( BIT(3) )
#define BIT_DISP_MTX_SOFT_RST					( BIT(2) )
#define BIT_DISPC_SOFT_RST					( BIT(1) )
#define BIT_DSI_SOFT_RST					( BIT(0) )
#define BIT_DISPC0_SOFT_RST					(BIT_DISPC_SOFT_RST)

/* bits definitions for register REG_AP_AHB_APCPU_RST_SET */
#define BIT_AP_QCHN_BIG_SOFT_RST				( BIT(17) )
#define BIT_AP_QCHN_LIT_SOFT_RST				( BIT(16) )
#define BIT_APCPU_GIC_SOFT_RST					( BIT(15) )
#define BIT_APCPU_CS_DBG_SOFT_RST				( BIT(14) )
#define BIT_APCPU_SOCDBG_SOFT_RST				( BIT(12) )
#define BITS_APCPU_ETM_SOFT_RST(_x_)				( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_APCPU_DBG_SOFT_RST(_x_)	( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_APCPU_CORE_SOFT_RST(_x_)	( (_x_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )
#define BIT_CA7_CS_DBG_SOFT_RST         BIT_APCPU_CS_DBG_SOFT_RST
#define BIT_CA7_SOCDBG_SOFT_RST         BIT_APCPU_SOCDBG_SOFT_RST
#define BITS_CA7_ETM_SOFT_RST(_x_)      BITS_APCPU_ETM_SOFT_RST(_x_)
#define BITS_CA7_DBG_SOFT_RST(_x_)      BITS_APCPU_DBG_SOFT_RST(_x_)
#define BITS_CA7_CORE_SOFT_RST(_x_)     BITS_APCPU_CORE_SOFT_RST(_x_)

/* bits definitions for register REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG */
#define BIT_APCPU_C3_AUTO_SLP_EN				( BIT(15) )
#define BIT_APCPU_C2_AUTO_SLP_EN				( BIT(14) )
#define BIT_APCPU_C1_AUTO_SLP_EN				( BIT(13) )
#define BIT_APCPU_C0_AUTO_SLP_EN				( BIT(12) )
#define BIT_APCPU_C3_WFI_SHUTDOWN_EN				( BIT(11) )
#define BIT_APCPU_C2_WFI_SHUTDOWN_EN				( BIT(10) )
#define BIT_APCPU_C1_WFI_SHUTDOWN_EN				( BIT(9) )
#define BIT_APCPU_C0_WFI_SHUTDOWN_EN				( BIT(8) )
#define BIT_MCU_APCPU_C3_SLEEP					( BIT(7) )
#define BIT_MCU_APCPU_C2_SLEEP					( BIT(6) )
#define BIT_MCU_APCPU_C1_SLEEP					( BIT(5) )
#define BIT_MCU_APCPU_C0_SLEEP					( BIT(4) )
#define BIT_CA7_C3_AUTO_SLP_EN          BIT_APCPU_C3_AUTO_SLP_EN
#define BIT_CA7_C2_AUTO_SLP_EN          BIT_APCPU_C2_AUTO_SLP_EN
#define BIT_CA7_C1_AUTO_SLP_EN          BIT_APCPU_C1_AUTO_SLP_EN
#define BIT_CA7_C0_AUTO_SLP_EN          BIT_APCPU_C0_AUTO_SLP_EN
#define BIT_CA7_C3_WFI_SHUTDOWN_EN      BIT_APCPU_C3_WFI_SHUTDOWN_EN
#define BIT_CA7_C2_WFI_SHUTDOWN_EN      BIT_APCPU_C2_WFI_SHUTDOWN_EN
#define BIT_CA7_C1_WFI_SHUTDOWN_EN      BIT_APCPU_C1_WFI_SHUTDOWN_EN
#define BIT_CA7_C0_WFI_SHUTDOWN_EN      BIT_APCPU_C0_WFI_SHUTDOWN_EN
#define BIT_MCU_CA7_C3_SLEEP            BIT_MCU_APCPU_C3_SLEEP
#define BIT_MCU_CA7_C2_SLEEP            BIT_MCU_APCPU_C2_SLEEP
#define BIT_MCU_CA7_C1_SLEEP            BIT_MCU_APCPU_C1_SLEEP
#define BIT_MCU_CA7_C0_SLEEP            BIT_MCU_APCPU_C0_SLEEP
#define BIT_CA53_C3_AUTO_SLP_EN          BIT_APCPU_C3_AUTO_SLP_EN
#define BIT_CA53_C2_AUTO_SLP_EN          BIT_APCPU_C2_AUTO_SLP_EN
#define BIT_CA53_C1_AUTO_SLP_EN          BIT_APCPU_C1_AUTO_SLP_EN
#define BIT_CA53_C0_AUTO_SLP_EN          BIT_APCPU_C0_AUTO_SLP_EN
#define BIT_CA53_C3_WFI_SHUTDOWN_EN      BIT_APCPU_C3_WFI_SHUTDOWN_EN
#define BIT_CA53_C2_WFI_SHUTDOWN_EN      BIT_APCPU_C2_WFI_SHUTDOWN_EN
#define BIT_CA53_C1_WFI_SHUTDOWN_EN      BIT_APCPU_C1_WFI_SHUTDOWN_EN
#define BIT_CA53_C0_WFI_SHUTDOWN_EN      BIT_APCPU_C0_WFI_SHUTDOWN_EN
#define BIT_MCU_CA53_C3_SLEEP            BIT_MCU_APCPU_C3_SLEEP
#define BIT_MCU_CA53_C2_SLEEP            BIT_MCU_APCPU_C2_SLEEP
#define BIT_MCU_CA53_C1_SLEEP            BIT_MCU_APCPU_C1_SLEEP
#define BIT_MCU_CA53_C0_SLEEP            BIT_MCU_APCPU_C0_SLEEP
#define BIT_AP_PERI_FORCE_ON					( BIT(2) )
#define BIT_AP_PERI_FORCE_SLP					( BIT(1) )
#define BIT_AP_APB_SLEEP					( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG */
#define BIT_GSP_CKG_FORCE_EN					( BIT(9) )
#define BIT_GSP_AUTO_GATE_EN					( BIT(8) )
#define BIT_APCPU_TRACE_FORCE_SLEEP				( BIT(7) )
#define BIT_APCPU_TRACE_AUTO_GATE_EN				( BIT(6) )
#define BIT_AP_AHB_AUTO_GATE_EN					( BIT(5) )
#define BIT_AP_EMC_AUTO_GATE_EN					( BIT(4) )
#define BIT_APCPU_EMC_AUTO_GATE_EN				( BIT(3) )
#define BIT_APCPU_DBG_FORCE_SLEEP				( BIT(2) )
#define BIT_APCPU_DBG_AUTO_GATE_EN				( BIT(1) )
#define BIT_APCPU_CORE_AUTO_GATE_EN				( BIT(0) )
#define BIT_CA7_EMC_AUTO_GATE_EN        BIT_APCPU_EMC_AUTO_GATE_EN
#define BIT_CA7_DBG_FORCE_SLEEP         BIT_APCPU_DBG_FORCE_SLEEP
#define BIT_CA7_DBG_AUTO_GATE_EN        BIT_APCPU_DBG_AUTO_GATE_EN
#define BIT_CA7_CORE_AUTO_GATE_EN       BIT_APCPU_CORE_AUTO_GATE_EN
#define BIT_CA53_EMC_AUTO_GATE_EN        BIT_APCPU_EMC_AUTO_GATE_EN
#define BIT_CA53_DBG_FORCE_SLEEP         BIT_APCPU_DBG_FORCE_SLEEP
#define BIT_CA53_DBG_AUTO_GATE_EN        BIT_APCPU_DBG_AUTO_GATE_EN
#define BIT_CA53_CORE_AUTO_GATE_EN       BIT_APCPU_CORE_AUTO_GATE_EN

/* bits definitions for register REG_AP_AHB_HOLDING_PEN */
#define BITS_HOLDING_PEN(_x_)           ( (_x_) << 0 )

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C0 */
#define BITS_JMP_ADDR_APCPU_C0(_x_)	(_x_)
#define BITS_JMP_ADDR_CA7_C0(_x_)       BITS_JMP_ADDR_APCPU_C0

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C1 */
#define BITS_JMP_ADDR_APCPU_C1(_x_)	(_x_)
#define BITS_JMP_ADDR_CA7_C1(_x_)       BITS_JMP_ADDR_APCPU_C1(_x_)

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C2 */
#define BITS_JMP_ADDR_APCPU_C2(_x_)	(_x_)
#define BITS_JMP_ADDR_CA7_C2(_x_)       BITS_JMP_ADDR_APCPU_C2(_x_)

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C3 */
#define BITS_JMP_ADDR_APCPU_C3(_x_)	(_x_)
#define BITS_JMP_ADDR_CA7_C3(_x_)       BITS_JMP_ADDR_APCPU_C3(_x_)

/* bits definitions for register REG_AP_AHB_CA7_C0_PU_LOCK */
#define BIT_APCPU_C0_PU_LOCK		( BIT(0) )
#define BIT_CA7_C0_PU_LOCK             BIT_APCPU_C0_PU_LOCK
#define BIT_CA53_C0_PU_LOCK             BIT_APCPU_C0_PU_LOCK

/* bits definitions for register REG_AP_AHB_CA7_C1_PU_LOCK */
#define BIT_APCPU_C1_PU_LOCK		( BIT(0) )
#define BIT_CA7_C1_PU_LOCK              BIT_APCPU_C1_PU_LOCK
#define BIT_CA53_C1_PU_LOCK              BIT_APCPU_C1_PU_LOCK

/* bits definitions for register REG_AP_AHB_CA7_C2_PU_LOCK */
#define BIT_APCPU_C2_PU_LOCK		( BIT(0) )
#define BIT_CA7_C2_PU_LOCK             BIT_APCPU_C2_PU_LOCK
#define BIT_CA53_C2_PU_LOCK             BIT_APCPU_C2_PU_LOCK

/* bits definitions for register REG_AP_AHB_CA7_C3_PU_LOCK */
#define BIT_APCPU_C3_PU_LOCK		( BIT(0) )
#define BIT_CA7_C3_PU_LOCK              BIT_APCPU_C3_PU_LOCK
#define BIT_CA53_C3_PU_LOCK              BIT_APCPU_C3_PU_LOCK

/* bits definitions for register REG_AP_AHB_APCPU_CKG_DIV_CFG */
#define BITS_APCPU_LIT_ATB_CKG_DIV(_X_)				( (_X_) << 23 & (BIT(23)|BIT(24)|BIT(25)) )
#define BITS_APCPU_BIG_ATB_CKG_DIV(_X_)				( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_APCPU_LIT_DBG_CKG_DIV(_X_)				( (_X_) << 17 & (BIT(17)|BIT(18)|BIT(19)) )
#define BITS_APCPU_BIG_DBG_CKG_DIV(_X_)				( (_X_) << 14 & (BIT(14)|BIT(15)|BIT(16)) )
#define BITS_APCPU_LIT_ACE_CKG_DIV(_X_)				( (_X_) << 11 & (BIT(11)|BIT(12)|BIT(13)) )
#define BITS_APCPU_BIG_ACE_CKG_DIV(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_APCPU_LIT_MCU_CKG_DIV(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_APCPU_BIG_MCU_CKG_DIV(_X_)				( (_X_) << 1 & (BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_MCU_PAUSE */
#define BIT_DMA_ACT_LIGHT_EN					( BIT(5) )
#define BIT_MCU_SLEEP_FOLLOW_APCPU_EN			( BIT(4) )
#define BIT_MCU_SLEEP_FOLLOW_CA7_EN     		BIT_MCU_SLEEP_FOLLOW_APCPU_EN
#define BIT_MCU_SLEEP_FOLLOW_CA53_EN                     BIT_MCU_SLEEP_FOLLOW_APCPU_EN
#define BIT_MCU_LIGHT_SLEEP_EN					( BIT(3) )
#define BIT_MCU_DEEP_SLEEP_EN					( BIT(2) )
#define BIT_MCU_SYS_SLEEP_EN					( BIT(1) )
#define BIT_MCU_CORE_SLEEP					( BIT(0) )

/* bits definitions for register REG_AP_AHB_MISC_CKG_EN */
#define BIT_ASHB_APCPU_DBG_VLD					( BIT(9) )
#define BIT_ASHB_APCPU_DBG_EN					( BIT(8) )
#define BIT_ASHB_CA7_DBG_VLD            BIT_ASHB_APCPU_DBG_VLD
#define BIT_ASHB_CA7_DBG_EN             BIT_ASHB_APCPU_DBG_EN
#define BIT_ASHB_CA53_DBG_VLD            BIT_ASHB_APCPU_DBG_VLD
#define BIT_ASHB_CA53_DBG_EN             BIT_ASHB_APCPU_DBG_EN
#define BIT_DISP_TMC_CKG_EN					( BIT(4) )
#define BIT_DPHY_REF_CKG_EN					( BIT(1) )
#define BIT_DPHY_CFG_CKG_EN					( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C0_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_APCPU_C0_AUTO_FORCE_SHUTDOWN_EN	( BIT(0) )
#define BIT_CA7_C0_AUTO_FORCE_SHUTDOWN_EN BIT_APCPU_C0_AUTO_FORCE_SHUTDOWN_EN
#define BIT_CA53_C0_AUTO_FORCE_SHUTDOWN_EN BIT_APCPU_C0_AUTO_FORCE_SHUTDOWN_EN

/* bits definitions for register REG_AP_AHB_CA7_C1_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_APCPU_C1_AUTO_FORCE_SHUTDOWN_EN	(	 BIT(0) )
#define BIT_CA7_C1_AUTO_FORCE_SHUTDOWN_EN BIT_APCPU_C1_AUTO_FORCE_SHUTDOWN_EN	
#define BIT_CA53_C1_AUTO_FORCE_SHUTDOWN_EN BIT_APCPU_C1_AUTO_FORCE_SHUTDOWN_EN

/* bits definitions for register REG_AP_AHB_CA7_C2_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_APCPU_C2_AUTO_FORCE_SHUTDOWN_EN	( BIT(0) )
#define BIT_CA7_C2_AUTO_FORCE_SHUTDOWN_EN BIT_APCPU_C2_AUTO_FORCE_SHUTDOWN_EN
#define BIT_CA53_C2_AUTO_FORCE_SHUTDOWN_EN BIT_APCPU_C2_AUTO_FORCE_SHUTDOWN_EN

/* bits definitions for register REG_AP_AHB_CA7_C3_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_APCPU_C3_AUTO_FORCE_SHUTDOWN_EN	(	 BIT(0) )
#define BIT_CA7_C3_AUTO_FORCE_SHUTDOWN_EN BIT_APCPU_C3_AUTO_FORCE_SHUTDOWN_EN
#define BIT_CA53_C3_AUTO_FORCE_SHUTDOWN_EN BIT_APCPU_C3_AUTO_FORCE_SHUTDOWN_EN

/* bits definitions for register REG_AP_AHB_APCPU_CKG_SEL_CFG */
#define BITS_APCPU_LIT_MCU_CKG_SEL(_X_)				( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_APCPU_BIG_MCU_CKG_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_APCPU_AUTO_GATE_EN */
#define BIT_APCPU_AUTO_GATE_EN					( BIT(0) )

/* bits definitions for register REG_AP_AHB_APCPU_GIC_CKG_CFG */
#define BIT_APCPU_GIC_CKG_EN					( BIT(0) )

/* bits definitions for register REG_AP_AHB_APCPU_GIC_CLK_CFG */
#define BITS_APCPU_GIC_CLK_DIV(_X_)				( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_APCPU_GIC_CLK_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AP_AHB_APCPU_LIT_CKG_EN */
#define BIT_APCPU_LIT_ATB_EN					( BIT(2) )
#define BIT_APCPU_LIT_DBG_EN					( BIT(1) )
#define BIT_APCPU_LIT_CORE_EN					( BIT(0) )

/* bits definitions for register REG_AP_AHB_APCPU_BIG_CKG_EN */
#define BIT_APCPU_BIG_ATB_EN					( BIT(2) )
#define BIT_APCPU_BIG_DBG_EN					( BIT(1) )
#define BIT_APCPU_BIG_CORE_EN					( BIT(0) )

/* bits definitions for register REG_AP_AHB_APCPU_LIT_L2FLUSH */
#define BIT_APCPU_LIT_L2FLUSHDONE				( BIT(1) )
#define BIT_APCPU_LIT_L2FLUSHREQ				( BIT(0) )

/* bits definitions for register REG_AP_AHB_APCPU_BIG_L2FLUSH */
#define BIT_APCPU_BIG_L2FLUSHDONE				( BIT(1) )
#define BIT_APCPU_BIG_L2FLUSHREQ				( BIT(0) )

/* bits definitions for register REG_AP_AHB_APCPU_CCI_BASE_H25 */
#define BITS_APCPU_CCI_BASE_H25(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)) )

/* bits definitions for register REG_AP_AHB_APCPU_CCI_CTRL */
#define BITS_APCPU_CCI_ARQOS_LIT(_X_)				( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_APCPU_CCI_AWQOS_LIT(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_APCPU_CCI_ARQOS_BIG(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_APCPU_CCI_AWQOS_BIG(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_APCPU_CCI_BUFOVRD(_X_)				( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)) )
#define BITS_APCPU_CCI_QOSOVRD(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register REG_AP_AHB_MISC_CFG */
#define BIT_APCPU_DAP_SNOOP_DISABLE				( BIT(25) )
#define BIT_APCPU_BIG_INT_DISABLE				( BIT(24) )
#define BIT_APCPU_IRAM_REMAP					( BIT(23) )
#define BIT_APCPU_L2RAM_LIGHT_SLEEP_EN				( BIT(22) )
#define BIT_APCPU_AUTO_REG_SAVE_TRIG_SEL			( BIT(21) )
#define BIT_APCPU_AUTO_REG_SAVE_EN				( BIT(20) )
#define BITS_EMMC_SLOT_SEL(_X_)					( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_SDIO0_SLOT_SEL(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_BUSMON2_CHN_SEL(_X_)				( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_BUSMON1_CHN_SEL(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_BUSMON0_CHN_SEL(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_SDIO2_SLOT_SEL(_X_)				( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_SDIO1_SLOT_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AP_AHB_AP_MAIN_MTX_HPROT_CFG */
#define BITS_HPROT_NFC(_X_)					( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_HPROT_EMMC(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_HPROT_SDIO2(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_HPROT_SDIO1(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HPROT_SDIO0(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_HPROT_DMAW(_X_)					( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_HPROT_DMAR(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_APCPU_STANDBY_STATUS */
#define BIT_STANDBYWFIL2_BIG					( BIT(24) )
#define BITS_STANDBYWFE_BIG(_X_)				( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_STANDBYWFI_BIG(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BIT_STANDBYWFIL2_LIT					( BIT(8) )
#define BITS_STANDBYWFE_LIT(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_STANDBYWFI_LIT(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_NANC_CLK_CFG */
#define BITS_CLK_NANDC2X_DIV(_X_)				( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_CLK_NANDC2X_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AP_AHB_LVDS_CFG */
#define BITS_LVDS_TXCLKDATA(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)) )
#define BITS_LVDS_TXCOM(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_LVDS_TXSLEW(_X_)					( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_LVDS_TXSW(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_LVDS_TXRERSER(_X_)					( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LVDS_PRE_EMP(_X_)					( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_LVDS_TXPD						( BIT(0) )

/* bits definitions for register REG_AP_AHB_LVDS_PLL_CFG0 */
#define BIT_LVDS_PLL_LOCK_DET					( BIT(31) )
#define BITS_LVDS_PLL_REFIN(_X_)				( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_LVDS_PLL_LPF(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BIT_LVDS_PLL_DIV_S					( BIT(18) )
#define BITS_LVDS_PLL_IBIAS(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_LVDS_PLLN(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AP_AHB_LVDS_PLL_CFG1 */
#define BITS_LVDS_PLL_KINT(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_LVDS_PLL_RSV(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_LVDS_PLL_MOD_EN					( BIT(7) )
#define BIT_LVDS_PLL_SDM_EN					( BIT(6) )
#define BITS_LVDS_PLL_NINT(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AP_AHB_AP_QOS_CFG */
#define BITS_QOS_R_TMC(_X_)					( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_QOS_W_TMC(_X_)					( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_QOS_R_DISPC(_X_)					( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_QOS_W_DISPC(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_OTG_PHY_TUNE */
#define BIT_OTG_TXPREEMPPULSETUNE				( BIT(20) )
#define BITS_OTG_TXRESTUNE(_X_)					( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_OTG_TXHSXVTUNE(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_OTG_TXVREFTUNE(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_OTG_TXPREEMPAMPTUNE(_X_)				( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_OTG_TXRISETUNE(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_OTG_TXFSLSTUNE(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_OTG_SQRXTUNE(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_OTG_PHY_TEST */
#define BIT_OTG_ATERESET					( BIT(31) )
#define BIT_OTG_VBUS_VALID_EXT_SEL				( BIT(26) )
#define BIT_OTG_VBUS_VALID_EXT					( BIT(25) )
#define BIT_OTG_OTGDISABLE					( BIT(24) )
#define BIT_OTG_TESTBURNIN					( BIT(21) )
#define BIT_OTG_LOOPBACKENB					( BIT(20) )
#define BITS_OTG_TESTDATAOUT(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_OTG_VATESTENB(_X_)					( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BIT_OTG_TESTCLK						( BIT(13) )
#define BIT_OTG_TESTDATAOUTSEL					( BIT(12) )
#define BITS_OTG_TESTADDR(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_OTG_TESTDATAIN(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AP_AHB_OTG_PHY_CTRL */
#define BITS_OTG_SS_SCALEDOWNMODE(_X_)				( (_X_) << 25 & (BIT(25)|BIT(26)) )
#define BIT_OTG_TXBITSTUFFENH					( BIT(23) )
#define BIT_OTG_TXBITSTUFFEN					( BIT(22) )
#define BIT_OTG_DMPULLDOWN					( BIT(21) )
#define BIT_OTG_DPPULLDOWN					( BIT(20) )
#define BIT_OTG_DMPULLUP					( BIT(9) )
#define BIT_OTG_COMMONONN					( BIT(8) )
#define BITS_OTG_REFCLKSEL(_X_)					( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_OTG_FSEL(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_HSIC_PHY_TUNE */
#define BITS_HSIC_REFCLK_DIV(_X_)				( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)) )
#define BIT_HSIC_TXPREEMPPULSETUNE				( BIT(20) )
#define BITS_HSIC_TXRESTUNE(_X_)				( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_HSIC_TXHSXVTUNE(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_HSIC_TXVREFTUNE(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HSIC_TXPREEMPAMPTUNE(_X_)				( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_HSIC_TXRISETUNE(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_HSIC_TXFSLSTUNE(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_HSIC_SQRXTUNE(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_HSIC_PHY_TEST */
#define BIT_HSIC_ATERESET					( BIT(31) )
#define BIT_HSIC_VBUS_VALID_EXT_SEL				( BIT(26) )
#define BIT_HSIC_VBUS_VALID_EXT					( BIT(25) )
#define BIT_HSIC_OTGDISABLE					( BIT(24) )
#define BIT_HSIC_TESTBURNIN					( BIT(21) )
#define BIT_HSIC_LOOPBACKENB					( BIT(20) )
#define BITS_HSIC_TESTDATAOUT(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_HSIC_VATESTENB(_X_)				( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BIT_HSIC_TESTCLK					( BIT(13) )
#define BIT_HSIC_TESTDATAOUTSEL					( BIT(12) )
#define BITS_HSIC_TESTADDR(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_HSIC_TESTDATAIN(_X_)				( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AP_AHB_HSIC_PHY_CTRL */
#define BITS_HSIC_SS_SCALEDOWNMODE(_X_)				( (_X_) << 25 & (BIT(25)|BIT(26)) )
#define BIT_HSIC_TXBITSTUFFENH					( BIT(23) )
#define BIT_HSIC_TXBITSTUFFEN					( BIT(22) )
#define BIT_HSIC_DMPULLDOWN					( BIT(21) )
#define BIT_HSIC_DPPULLDOWN					( BIT(20) )
#define BIT_HSIC_IF_MODE					( BIT(16) )
#define BIT_IF_SELECT_HSIC					( BIT(13) )
#define BIT_HSIC_DBNCE_FLTR_BYPASS				( BIT(12) )
#define BIT_HSIC_DMPULLUP					( BIT(9) )
#define BIT_HSIC_COMMONONN					( BIT(8) )
#define BITS_HSIC_REFCLKSEL(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_HSIC_FSEL(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_ZIP_MTX_QOS_CFG */
#define BITS_ZIPMTX_S0_ARQOS(_X_)				( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_ZIPMTX_S0_AWQOS(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_ZIPDEC_ARQOS(_X_)					( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ZIPDEC_AWQOS(_X_)					( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_ZIPENC_ARQOS(_X_)					( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_ZIPENC_AWQOS(_X_)					( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_APCPU_QCHN_CTL_BIG */
#define BITS_APCPU_Q_NEG_THD_BIG(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_APCPU_Q_WAIT_GAP_BIG(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_APCPU_Q_PGEN_DUR_BIG(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_APCPU_Q_WAIT_REQ_BIG(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_APCPU_Q_RETN_BYP_BIG				( BIT(2) )
#define BIT_APCPU_Q_RETN_SEL_BIG				( BIT(1) )
#define BIT_APCPU_Q_LP_EN_BIG					( BIT(0) )

/* bits definitions for register REG_AP_AHB_APCPU_QCHN_STA_BIG */
#define BITS_APCPU_Q_STATUS_BIG(_X_)				(_X_)

/* bits definitions for register REG_AP_AHB_APCPU_QCHN_CTL_LIT */
#define BITS_APCPU_Q_NEG_THD_LIT(_X_)				( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_APCPU_Q_WAIT_GAP_LIT(_X_)				( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_APCPU_Q_PGEN_DUR_LIT(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_APCPU_Q_WAIT_REQ_LIT(_X_)				( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_APCPU_Q_RETN_BYP_LIT				( BIT(2) )
#define BIT_APCPU_Q_RETN_SEL_LIT				( BIT(1) )
#define BIT_APCPU_Q_LP_EN_LIT					( BIT(0) )

/* bits definitions for register REG_AP_AHB_APCPU_QCHN_STA_LIT */
#define BITS_APCPU_Q_STATUS_LIT(_X_)				(_X_)

/* bits definitions for register REG_AP_AHB_GSP_SEL */
#define BIT_GSP_VER_SEL						( BIT(0) )

/* bits definitions for register REG_AP_AHB_RES_REG */
#define BITS_RES_REG(_X_)					(_X_)

/* bits definitions for register REG_AP_AHB_CHIP_ID */
#define BITS_CHIP_ID(_X_)					(_X_)

#endif
