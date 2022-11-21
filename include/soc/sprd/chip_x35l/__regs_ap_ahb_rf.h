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


#ifndef __H_REGS_AP_AHB_HEADFILE_H__
#define __H_REGS_AP_AHB_HEADFILE_H__ __FILE__

#define	REGS_AP_AHB_RF

/* registers definitions for controller REGS_AP_AHB */
#define REG_AP_AHB_AHB_EB               SCI_ADDR(REGS_AP_AHB_BASE, 0x0000)
#define REG_AP_AHB_AHB_RST              SCI_ADDR(REGS_AP_AHB_BASE, 0x0004)
#define REG_AP_AHB_CA7_RST_SET          SCI_ADDR(REGS_AP_AHB_BASE, 0x0008)
#define REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG SCI_ADDR(REGS_AP_AHB_BASE, 0x000C)
#define REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG SCI_ADDR(REGS_AP_AHB_BASE, 0x0010)
#define REG_AP_AHB_HOLDING_PEN          SCI_ADDR(REGS_AP_AHB_BASE, 0x0014)
#define REG_AP_AHB_JMP_ADDR_CA7_C0      SCI_ADDR(REGS_AP_AHB_BASE, 0x0018)
#define REG_AP_AHB_JMP_ADDR_CA7_C1      SCI_ADDR(REGS_AP_AHB_BASE, 0x001C)
#define REG_AP_AHB_JMP_ADDR_CA7_C2      SCI_ADDR(REGS_AP_AHB_BASE, 0x0020)
#define REG_AP_AHB_JMP_ADDR_CA7_C3      SCI_ADDR(REGS_AP_AHB_BASE, 0x0024)
#define REG_AP_AHB_CA7_C0_PU_LOCK       SCI_ADDR(REGS_AP_AHB_BASE, 0x0028)
#define REG_AP_AHB_CA7_C1_PU_LOCK       SCI_ADDR(REGS_AP_AHB_BASE, 0x002C)
#define REG_AP_AHB_CA7_C2_PU_LOCK       SCI_ADDR(REGS_AP_AHB_BASE, 0x0030)
#define REG_AP_AHB_CA7_C3_PU_LOCK       SCI_ADDR(REGS_AP_AHB_BASE, 0x0034)
#define REG_AP_AHB_CA7_CKG_DIV_CFG      SCI_ADDR(REGS_AP_AHB_BASE, 0x0038)
#define REG_AP_AHB_MCU_PAUSE            SCI_ADDR(REGS_AP_AHB_BASE, 0x003C)
#define REG_AP_AHB_MISC_CKG_EN          SCI_ADDR(REGS_AP_AHB_BASE, 0x0040)
#define REG_AP_AHB_CA7_C0_AUTO_FORCE_SHUTDOWN_EN SCI_ADDR(REGS_AP_AHB_BASE, 0x0044)
#define REG_AP_AHB_CA7_C1_AUTO_FORCE_SHUTDOWN_EN SCI_ADDR(REGS_AP_AHB_BASE, 0x0048)
#define REG_AP_AHB_CA7_C2_AUTO_FORCE_SHUTDOWN_EN SCI_ADDR(REGS_AP_AHB_BASE, 0x004C)
#define REG_AP_AHB_CA7_C3_AUTO_FORCE_SHUTDOWN_EN SCI_ADDR(REGS_AP_AHB_BASE, 0x0050)
#define REG_AP_AHB_CA7_CKG_SEL_CFG      SCI_ADDR(REGS_AP_AHB_BASE, 0x0054)
#define REG_AP_AHB_MISC_CFG             SCI_ADDR(REGS_AP_AHB_BASE, 0x3000)
#define REG_AP_AHB_AP_MAIN_MTX_HPROT_CFG SCI_ADDR(REGS_AP_AHB_BASE, 0x3004)
#define REG_AP_AHB_CA7_STANDBY_STATUS   SCI_ADDR(REGS_AP_AHB_BASE, 0x3008)
#define REG_AP_AHB_NANC_CLK_CFG         SCI_ADDR(REGS_AP_AHB_BASE, 0x300C)
#define REG_AP_AHB_LVDS_CFG             SCI_ADDR(REGS_AP_AHB_BASE, 0x3010)
#define REG_AP_AHB_LVDS_PLL_CFG0        SCI_ADDR(REGS_AP_AHB_BASE, 0x3014)
#define REG_AP_AHB_LVDS_PLL_CFG1        SCI_ADDR(REGS_AP_AHB_BASE, 0x3018)
#define REG_AP_AHB_AP_QOS_CFG           SCI_ADDR(REGS_AP_AHB_BASE, 0x301C)
#define REG_AP_AHB_OTG_PHY_TUNE         SCI_ADDR(REGS_AP_AHB_BASE, 0x3020)
#define REG_AP_AHB_OTG_PHY_TEST         SCI_ADDR(REGS_AP_AHB_BASE, 0x3024)
#define REG_AP_AHB_OTG_PHY_CTRL         SCI_ADDR(REGS_AP_AHB_BASE, 0x3028)
#define REG_AP_AHB_HSIC_PHY_TUNE        SCI_ADDR(REGS_AP_AHB_BASE, 0x302C)
#define REG_AP_AHB_HSIC_PHY_TEST        SCI_ADDR(REGS_AP_AHB_BASE, 0x3030)
#define REG_AP_AHB_HSIC_PHY_CTRL        SCI_ADDR(REGS_AP_AHB_BASE, 0x3034)
#define REG_AP_AHB_ZIP_MTX_QOS_CFG      SCI_ADDR(REGS_AP_AHB_BASE, 0x3038)
#define REG_AP_AHB_CHIP_ID              SCI_ADDR(REGS_AP_AHB_BASE, 0x30FC)

/* bits definitions for register REG_AP_AHB_AHB_EB */
#define BIT_ZIPMTX_EB                   ( BIT(23) )
#define BIT_LVDS_EB                     ( BIT(22) )
#define BIT_ZIPDEC_EB                   ( BIT(21) )
#define BIT_ZIPENC_EB                   ( BIT(20) )
#define BIT_NANDC_ECC_EB                ( BIT(19) )
#define BIT_NANDC_2X_EB                 ( BIT(18) )
#define BIT_NANDC_EB                    ( BIT(17) )
#define BIT_BUSMON2_EB                  ( BIT(16) )
#define BIT_BUSMON1_EB                  ( BIT(15) )
#define BIT_BUSMON0_EB                  ( BIT(14) )
#define BIT_SPINLOCK_EB                 ( BIT(13) )
#define BIT_EMMC_EB                     ( BIT(11) )
#define BIT_SDIO2_EB                    ( BIT(10) )
#define BIT_SDIO1_EB                    ( BIT(9) )
#define BIT_SDIO0_EB                    ( BIT(8) )
#define BIT_DRM_EB                      ( BIT(7) )
#define BIT_NFC_EB                      ( BIT(6) )
#define BIT_DMA_EB                      ( BIT(5) )
#define BIT_OTG_EB                      ( BIT(4) )
#define BIT_USB_EB						(BIT_OTG_EB)
#define BIT_GSP_EB                      ( BIT(3) )
#define BIT_HSIC_EB                     ( BIT(2) )
#define BIT_DISPC_EB                    ( BIT(1) )
#define BIT_DISPC0_EB					(BIT_DISPC_EB)
#define BIT_DSI_EB                      ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AHB_RST */
#define BIT_HSIC_PHY_SOFT_RST           ( BIT(30) )
#define BIT_HSIC_UTMI_SOFT_RST          ( BIT(29) )
#define BIT_HSIC_SOFT_RST               ( BIT(28) )
#define BIT_LVDS_SOFT_RST               ( BIT(25) )
#define BIT_ZIP_MTX_SOFT_RST            ( BIT(24) )
#define BIT_ZIPDEC_SOFT_RST             ( BIT(23) )
#define BIT_ZIPENC_SOFT_RST             ( BIT(22) )
#define BIT_NANDC_SOFT_RST              ( BIT(20) )
#define BIT_BUSMON2_SOFT_RST            ( BIT(19) )
#define BIT_BUSMON1_SOFT_RST            ( BIT(18) )
#define BIT_BUSMON0_SOFT_RST            ( BIT(17) )
#define BIT_SPINLOCK_SOFT_RST           ( BIT(16) )
#define BIT_EMMC_SOFT_RST               ( BIT(14) )
#define BIT_SDIO2_SOFT_RST              ( BIT(13) )
#define BIT_SDIO1_SOFT_RST              ( BIT(12) )
#define BIT_SDIO0_SOFT_RST              ( BIT(11) )
#define BIT_DRM_SOFT_RST                ( BIT(10) )
#define BIT_NFC_SOFT_RST                ( BIT(9) )
#define BIT_DMA_SOFT_RST                ( BIT(8) )
#define BIT_OTG_PHY_SOFT_RST            ( BIT(6) )
#define BIT_OTG_UTMI_SOFT_RST           ( BIT(5) )
#define BIT_OTG_SOFT_RST                ( BIT(4) )
#define BIT_GSP_SOFT_RST                ( BIT(3) )
#define BIT_DISP_MTX_SOFT_RST           ( BIT(2) )
#define BIT_DISPC0_SOFT_RST             ( BIT(1) )
#define BIT_DSI_SOFT_RST                ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_RST_SET */
#define BIT_CA7_CS_DBG_SOFT_RST         ( BIT(14) )
#define BIT_CA7_L2_SOFT_RST             ( BIT(13) )
#define BIT_CA7_SOCDBG_SOFT_RST         ( BIT(12) )
#define BITS_CA7_ETM_SOFT_RST(_x_)      ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CA7_DBG_SOFT_RST(_x_)      ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CA7_CORE_SOFT_RST(_x_)     ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG */
#define BIT_CA7_C3_AUTO_SLP_EN          ( BIT(15) )
#define BIT_CA7_C2_AUTO_SLP_EN          ( BIT(14) )
#define BIT_CA7_C1_AUTO_SLP_EN          ( BIT(13) )
#define BIT_CA7_C0_AUTO_SLP_EN          ( BIT(12) )
#define BIT_CA7_C3_WFI_SHUTDOWN_EN      ( BIT(11) )
#define BIT_CA7_C2_WFI_SHUTDOWN_EN      ( BIT(10) )
#define BIT_CA7_C1_WFI_SHUTDOWN_EN      ( BIT(9) )
#define BIT_CA7_C0_WFI_SHUTDOWN_EN      ( BIT(8) )
#define BIT_MCU_CA7_C3_SLEEP            ( BIT(7) )
#define BIT_MCU_CA7_C2_SLEEP            ( BIT(6) )
#define BIT_MCU_CA7_C1_SLEEP            ( BIT(5) )
#define BIT_MCU_CA7_C0_SLEEP            ( BIT(4) )
#define BIT_AP_PERI_FORCE_ON            ( BIT(2) )
#define BIT_AP_PERI_FORCE_SLP           ( BIT(1) )
#define BIT_AP_APB_SLEEP                ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG */
#define BIT_GSP_CKG_FORCE_EN            ( BIT(9) )
#define BIT_GSP_AUTO_GATE_EN            ( BIT(8) )
#define BIT_AP_AHB_AUTO_GATE_EN         ( BIT(5) )
#define BIT_AP_EMC_AUTO_GATE_EN         ( BIT(4) )
#define BIT_CA7_EMC_AUTO_GATE_EN        ( BIT(3) )
#define BIT_CA7_DBG_FORCE_SLEEP         ( BIT(2) )
#define BIT_CA7_DBG_AUTO_GATE_EN        ( BIT(1) )
#define BIT_CA7_CORE_AUTO_GATE_EN       ( BIT(0) )

/* bits definitions for register REG_AP_AHB_HOLDING_PEN */
#define BITS_HOLDING_PEN(_x_)           ( (_x_) << 0 )

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C0 */
#define BITS_JMP_ADDR_CA7_C0(_x_)       ( (_x_) << 0 )

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C1 */
#define BITS_JMP_ADDR_CA7_C1(_x_)       ( (_x_) << 0 )

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C2 */
#define BITS_JMP_ADDR_CA7_C2(_x_)       ( (_x_) << 0 )

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C3 */
#define BITS_JMP_ADDR_CA7_C3(_x_)       ( (_x_) << 0 )

/* bits definitions for register REG_AP_AHB_CA7_C0_PU_LOCK */
#define BIT_CA7_C0_PU_LOCK              ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C1_PU_LOCK */
#define BIT_CA7_C1_PU_LOCK              ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C2_PU_LOCK */
#define BIT_CA7_C2_PU_LOCK              ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C3_PU_LOCK */
#define BIT_CA7_C3_PU_LOCK              ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_CKG_DIV_CFG */
#define BITS_CA7_DBG_CKG_DIV(_x_)       ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)) )
#define BITS_CA7_AXI_CKG_DIV(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_CA7_MCU_CKG_DIV(_x_)       ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register REG_AP_AHB_MCU_PAUSE */
#define BIT_DMA_ACT_LIGHT_EN            ( BIT(5) )
#define BIT_MCU_SLEEP_FOLLOW_CA7_EN     ( BIT(4) )
#define BIT_MCU_LIGHT_SLEEP_EN          ( BIT(3) )
#define BIT_MCU_DEEP_SLEEP_EN           ( BIT(2) )
#define BIT_MCU_SYS_SLEEP_EN            ( BIT(1) )
#define BIT_MCU_CORE_SLEEP              ( BIT(0) )

/* bits definitions for register REG_AP_AHB_MISC_CKG_EN */
#define BIT_ASHB_CA7_DBG_VLD            ( BIT(9) )
#define BIT_ASHB_CA7_DBG_EN             ( BIT(8) )
#define BIT_DISP_TMC_CKG_EN             ( BIT(4) )
#define BIT_DPHY_REF_CKG_EN             ( BIT(1) )
#define BIT_DPHY_CFG_CKG_EN             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C0_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_CA7_C0_AUTO_FORCE_SHUTDOWN_EN ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C1_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_CA7_C1_AUTO_FORCE_SHUTDOWN_EN ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C2_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_CA7_C2_AUTO_FORCE_SHUTDOWN_EN ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C3_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_CA7_C3_AUTO_FORCE_SHUTDOWN_EN ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_CKG_SEL_CFG */
#define BITS_CA7_MCU_CKG_SEL(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_MISC_CFG */
#define BITS_EMMC_SLOT_SEL(_x_)         ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_SDIO0_SLOT_SEL(_x_)        ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_BUSMON2_CHN_SEL(_x_)       ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_BUSMON1_CHN_SEL(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_BUSMON0_CHN_SEL(_x_)       ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_SDIO2_SLOT_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_SDIO1_SLOT_SEL(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AP_AHB_AP_MAIN_MTX_HPROT_CFG */
#define BITS_HPROT_NFC(_x_)             ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_HPROT_EMMC(_x_)            ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_HPROT_SDIO2(_x_)           ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_HPROT_SDIO1(_x_)           ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HPROT_SDIO0(_x_)           ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_HPROT_DMAW(_x_)            ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_HPROT_DMAR(_x_)            ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_CA7_STANDBY_STATUS */
#define BIT_CA7_STANDBYWFIL2            ( BIT(12) )
#define BITS_CA7_ETMSTANDBYWFX(_x_)     ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CA7_STANDBYWFE(_x_)        ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CA7_STANDBYWFI(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_NANC_CLK_CFG */
#define BITS_CLK_NANDC2X_DIV(_x_)       ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_CLK_NANDC2X_SEL(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AP_AHB_LVDS_CFG */
#define BITS_LVDS_TXCLKDATA(_x_)        ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)) )
#define BITS_LVDS_TXCOM(_x_)            ( (_x_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_LVDS_TXSLEW(_x_)           ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_LVDS_TXSW(_x_)             ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_LVDS_TXRERSER(_x_)         ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LVDS_PRE_EMP(_x_)          ( (_x_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_LVDS_TXPD                   ( BIT(0) )

/* bits definitions for register REG_AP_AHB_LVDS_PLL_CFG0 */
#define BIT_LVDS_PLL_LOCK_DET           ( BIT(31) )
#define BITS_LVDS_PLL_REFIN(_x_)        ( (_x_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_LVDS_PLL_LPF(_x_)          ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BIT_LVDS_PLL_DIV_S              ( BIT(18) )
#define BITS_LVDS_PLL_IBIAS(_x_)        ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_LVDS_PLLN(_x_)             ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AP_AHB_LVDS_PLL_CFG1 */
#define BITS_LVDS_PLL_KINT(_x_)         ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_LVDS_PLL_RSV(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_LVDS_PLL_MOD_EN             ( BIT(7) )
#define BIT_LVDS_PLL_SDM_EN             ( BIT(6) )
#define BITS_LVDS_PLL_NINT(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AP_AHB_AP_QOS_CFG */
#define BITS_QOS_R_TMC(_x_)             ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_QOS_W_TMC(_x_)             ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_QOS_R_DISPC(_x_)           ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_QOS_W_DISPC(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_OTG_PHY_TUNE */
#define BIT_OTG_TXPREEMPPULSETUNE       ( BIT(20) )
#define BITS_OTG_TXRESTUNE(_x_)         ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_OTG_TXHSXVTUNE(_x_)        ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_OTG_TXVREFTUNE(_x_)        ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_OTG_TXPREEMPAMPTUNE(_x_)   ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_OTG_TXRISETUNE(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_OTG_TXFSLSTUNE(_x_)        ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_OTG_SQRXTUNE(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_OTG_PHY_TEST */
#define BIT_OTG_ATERESET                ( BIT(31) )
#define BIT_OTG_VBUS_VALID_EXT_SEL      ( BIT(26) )
#define BIT_OTG_VBUS_VALID_EXT          ( BIT(25) )
#define BIT_OTG_OTGDISABLE              ( BIT(24) )
#define BIT_OTG_TESTBURNIN              ( BIT(21) )
#define BIT_OTG_LOOPBACKENB             ( BIT(20) )
#define BITS_OTG_TESTDATAOUT(_x_)       ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_OTG_VATESTENB(_x_)         ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BIT_OTG_TESTCLK                 ( BIT(13) )
#define BIT_OTG_TESTDATAOUTSEL          ( BIT(12) )
#define BITS_OTG_TESTADDR(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_OTG_TESTDATAIN(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AP_AHB_OTG_PHY_CTRL */
#define BITS_OTG_SS_SCALEDOWNMODE(_x_)  ( (_x_) << 25 & (BIT(25)|BIT(26)) )
#define BIT_OTG_TXBITSTUFFENH           ( BIT(23) )
#define BIT_OTG_TXBITSTUFFEN            ( BIT(22) )
#define BIT_OTG_DMPULLDOWN              ( BIT(21) )
#define BIT_OTG_DPPULLDOWN              ( BIT(20) )
#define BIT_OTG_DMPULLUP                ( BIT(9) )
#define BIT_OTG_COMMONONN               ( BIT(8) )
#define BITS_OTG_REFCLKSEL(_x_)         ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_OTG_FSEL(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_HSIC_PHY_TUNE */
#define BITS_HSIC_REFCLK_DIV(_x_)       ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)) )
#define BIT_HSIC_TXPREEMPPULSETUNE      ( BIT(20) )
#define BITS_HSIC_TXRESTUNE(_x_)        ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_HSIC_TXHSXVTUNE(_x_)       ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_HSIC_TXVREFTUNE(_x_)       ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HSIC_TXPREEMPAMPTUNE(_x_)  ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_HSIC_TXRISETUNE(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_HSIC_TXFSLSTUNE(_x_)       ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_HSIC_SQRXTUNE(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_HSIC_PHY_TEST */
#define BIT_HSIC_ATERESET               ( BIT(31) )
#define BIT_HSIC_VBUS_VALID_EXT_SEL     ( BIT(26) )
#define BIT_HSIC_VBUS_VALID_EXT         ( BIT(25) )
#define BIT_HSIC_OTGDISABLE             ( BIT(24) )
#define BIT_HSIC_TESTBURNIN             ( BIT(21) )
#define BIT_HSIC_LOOPBACKENB            ( BIT(20) )
#define BITS_HSIC_TESTDATAOUT(_x_)      ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_HSIC_VATESTENB(_x_)        ( (_x_) << 14 & (BIT(14)|BIT(15)) )
#define BIT_HSIC_TESTCLK                ( BIT(13) )
#define BIT_HSIC_TESTDATAOUTSEL         ( BIT(12) )
#define BITS_HSIC_TESTADDR(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_HSIC_TESTDATAIN(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AP_AHB_HSIC_PHY_CTRL */
#define BITS_HSIC_SS_SCALEDOWNMODE(_x_) ( (_x_) << 25 & (BIT(25)|BIT(26)) )
#define BIT_HSIC_TXBITSTUFFENH          ( BIT(23) )
#define BIT_HSIC_TXBITSTUFFEN           ( BIT(22) )
#define BIT_HSIC_DMPULLDOWN             ( BIT(21) )
#define BIT_HSIC_DPPULLDOWN             ( BIT(20) )
#define BIT_HSIC_IF_MODE                ( BIT(16) )
#define BIT_IF_SELECT_HSIC              ( BIT(13) )
#define BIT_HSIC_DBNCE_FLTR_BYPASS      ( BIT(12) )
#define BIT_HSIC_DMPULLUP               ( BIT(9) )
#define BIT_HSIC_COMMONONN              ( BIT(8) )
#define BITS_HSIC_REFCLKSEL(_x_)        ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_HSIC_FSEL(_x_)             ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_ZIP_MTX_QOS_CFG */
#define BITS_ZIPMTX_S0_ARQOS(_x_)       ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_ZIPMTX_S0_AWQOS(_x_)       ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_ZIPDEC_ARQOS(_x_)          ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ZIPDEC_AWQOS(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_ZIPENC_ARQOS(_x_)          ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_ZIPENC_AWQOS(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_CHIP_ID */
#define BITS_CHIP_ID(_x_)               ( (_x_) << 0 )

/* vars definitions for controller REGS_AP_AHB */

#endif /* __REGS_AP_AHB_H__ */
