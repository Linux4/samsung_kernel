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

#ifndef __REGS_AP_AHB_H__
#define __REGS_AP_AHB_H__

#ifndef __SCI_GLB_REGS_H__
#error  "Don't include this file directly, include <mach/sci_glb_regs.h>"
#endif

#define REGS_AP_AHB

/* registers definitions for controller REGS_AP_AHB */
#define REG_AP_AHB_AHB_EB               SCI_ADDR(REGS_AP_AHB_BASE, 0x0000)
#define REG_AP_AHB_AHB_RST              SCI_ADDR(REGS_AP_AHB_BASE, 0x0004)
#define REG_AP_AHB_CA7_RST_SET          SCI_ADDR(REGS_AP_AHB_BASE, 0x0008)
#define REG_AP_AHB_CA7_CKG_CFG          SCI_ADDR(REGS_AP_AHB_BASE, 0x000C)
#define REG_AP_AHB_MCU_PAUSE            SCI_ADDR(REGS_AP_AHB_BASE, 0x0010)
#define REG_AP_AHB_MISC_CKG_EN          SCI_ADDR(REGS_AP_AHB_BASE, 0x0014)
#define REG_AP_AHB_MISC_CFG             SCI_ADDR(REGS_AP_AHB_BASE, 0x0018)
#define REG_AP_AHB_AP_MTX_S3_PRIO0      SCI_ADDR(REGS_AP_AHB_BASE, 0x001C)
#define REG_AP_AHB_AP_MTX_S3_PRIO1      SCI_ADDR(REGS_AP_AHB_BASE, 0x0020)
#define REG_AP_AHB_AP_MTX_S3_PRIO2      SCI_ADDR(REGS_AP_AHB_BASE, 0x0024)
#define REG_AP_AHB_AP_MTX_S2_PRIO0      SCI_ADDR(REGS_AP_AHB_BASE, 0x0028)
#define REG_AP_AHB_AP_MTX_S1_PRIO0      SCI_ADDR(REGS_AP_AHB_BASE, 0x002C)
#define REG_AP_AHB_AP_MTX_S0_PRIO0      SCI_ADDR(REGS_AP_AHB_BASE, 0x0030)
#define REG_AP_AHB_AP_MTX_S0_PRIO1      SCI_ADDR(REGS_AP_AHB_BASE, 0x0034)
#define REG_AP_AHB_AP_MTX_S0_PRIO2      SCI_ADDR(REGS_AP_AHB_BASE, 0x0038)
#define REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG SCI_ADDR(REGS_AP_AHB_BASE, 0x003C)
#define REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG SCI_ADDR(REGS_AP_AHB_BASE, 0x0040)
#define REG_AP_AHB_AP_MAIN_MTX_HPROT_CFG SCI_ADDR(REGS_AP_AHB_BASE, 0x0044)
#define REG_AP_AHB_CA7_STANDBY_STATUS   SCI_ADDR(REGS_AP_AHB_BASE, 0x0048)
#define REG_AP_AHB_HOLDING_PEN          SCI_ADDR(REGS_AP_AHB_BASE, 0x004C)
#define REG_AP_AHB_JMP_ADDR_CA7_C0      SCI_ADDR(REGS_AP_AHB_BASE, 0x0050)
#define REG_AP_AHB_JMP_ADDR_CA7_C1      SCI_ADDR(REGS_AP_AHB_BASE, 0x0054)
#define REG_AP_AHB_JMP_ADDR_CA7_C2      SCI_ADDR(REGS_AP_AHB_BASE, 0x0058)
#define REG_AP_AHB_JMP_ADDR_CA7_C3      SCI_ADDR(REGS_AP_AHB_BASE, 0x005C)
#define REG_AP_AHB_CHIP_ID              SCI_ADDR(REGS_AP_AHB_BASE, 0x00FC)

/* bits definitions for register REG_AP_AHB_AHB_EB */
#define BIT_BUSMON2_EB                  ( BIT(16) )
#define BIT_BUSMON1_EB                  ( BIT(15) )
#define BIT_BUSMON0_EB                  ( BIT(14) )
#define BIT_SPINLOCK_EB                 ( BIT(13) )
#define BIT_GPS_EB                      ( BIT(12) )
#define BIT_EMMC_EB                     ( BIT(11) )
#define BIT_SDIO2_EB                    ( BIT(10) )
#define BIT_SDIO1_EB                    ( BIT(9) )
#define BIT_SDIO0_EB                    ( BIT(8) )
#define BIT_DRM_EB                      ( BIT(7) )
#define BIT_NFC_EB                      ( BIT(6) )
#define BIT_DMA_EB                      ( BIT(5) )
#define BIT_USB_EB                      ( BIT(4) )
#define BIT_GSP_EB                      ( BIT(3) )
#define BIT_DISPC1_EB                   ( BIT(2) )
#define BIT_DISPC0_EB                   ( BIT(1) )
#define BIT_DSI_EB                      ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AHB_RST */
#define BIT_BUSMON2_SOFT_RST            ( BIT(19) )
#define BIT_BUSMON1_SOFT_RST            ( BIT(18) )
#define BIT_BUSMON0_SOFT_RST            ( BIT(17) )
#define BIT_SPINLOCK_SOFT_RST           ( BIT(16) )
#define BIT_GPS_SOFT_RST                ( BIT(15) )
#define BIT_EMMC_SOFT_RST               ( BIT(14) )
#define BIT_SDIO2_SOFT_RST              ( BIT(13) )
#define BIT_SDIO1_SOFT_RST              ( BIT(12) )
#define BIT_SDIO0_SOFT_RST              ( BIT(11) )
#define BIT_DRM_SOFT_RST                ( BIT(10) )
#define BIT_NFC_SOFT_RST                ( BIT(9) )
#define BIT_DMA_SOFT_RST                ( BIT(8) )
#define BIT_USB_PHY_SOFT_RST            ( BIT(7) )
#define BIT_USB_UTMI_SOFT_RST           ( BIT(6) )
#define BIT_USB_SOFT_RST                ( BIT(5) )
#define BIT_DISP_MTX_SOFT_RST           ( BIT(4) )
#define BIT_GSP_SOFT_RST                ( BIT(3) )
#define BIT_DISPC1_SOFT_RST             ( BIT(2) )
#define BIT_DISPC0_SOFT_RST             ( BIT(1) )
#define BIT_DSI_SOFT_RST                ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_RST_SET */
#define BIT_CA7_CS_DBG_SOFT_RST         ( BIT(14) )
#define BIT_CA7_L2_SOFT_RST             ( BIT(13) )
#define BIT_CA7_SOCDBG_SOFT_RST         ( BIT(12) )
#define BITS_CA7_ETM_SOFT_RST(_x_)      ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CA7_DBG_SOFT_RST(_x_)      ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CA7_CORE_SOFT_RST(_x_)     ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_CA7_CKG_CFG */
#define BITS_CA7_DBG_CKG_DIV(_x_)       ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)) )
#define BITS_CA7_AXI_CKG_DIV(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_CA7_MCU_CKG_DIV(_x_)       ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_CA7_MCU_CKG_SEL(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_MCU_PAUSE */
#define BIT_DMA_ACT_LIGHT_EN            ( BIT(5) )
#define BIT_MCU_SLEEP_FOLLOW_CA7_EN     ( BIT(4) )
#define BIT_MCU_LIGHT_SLEEP_EN          ( BIT(3) )
#define BIT_MCU_DEEP_SLEEP_EN           ( BIT(2) )
#define BIT_MCU_SYS_SLEEP_EN            ( BIT(1) )
#define BIT_MCU_CORE_SLEEP              ( BIT(0) )

/* bits definitions for register REG_AP_AHB_MISC_CKG_EN */
#define BIT_GPS_TCXO_INV_SEL            ( BIT(13) )
#define BIT_GPS_26M_INV_SEL             ( BIT(12) )
#define BIT_ASHB_CA7_DBG_VLD            ( BIT(9) )
#define BIT_ASHB_CA7_DBG_EN             ( BIT(8) )
#define BIT_DISP_TMC_CKG_EN             ( BIT(4) )
#define BIT_DPHY_REF_CKG_EN             ( BIT(1) )
#define BIT_DPHY_CFG_CKG_EN             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_MISC_CFG */
#define BIT_EMMC_SLOT_SEL               ( BIT(17) )
#define BIT_SDIO0_SLOT_SEL              ( BIT(16) )
#define BITS_BUSMON2_CHN_SEL(_x_)       ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_BUSMON1_CHN_SEL(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_BUSMON0_CHN_SEL             ( BIT(4) )
#define BITS_SDIO2_SLOT_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_SDIO1_SLOT_SEL(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S3_PRIO0 */
#define BITS_PRI_M6TOS3_RND_THR(_x_)    ( (_x_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M6TOS3_SEL(_x_)        ( (_x_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M6TOS3_RND_EN           ( BIT(25) )
#define BIT_PRI_M6TOS3_ADJ_EN           ( BIT(24) )
#define BITS_PRI_M7TOS3_RND_THR(_x_)    ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M7TOS3_SEL(_x_)        ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M7TOS3_RND_EN           ( BIT(17) )
#define BIT_PRI_M7TOS3_ADJ_EN           ( BIT(16) )
#define BITS_PRI_M8TOS3_RND_THR(_x_)    ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M8TOS3_SEL(_x_)        ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M8TOS3_RND_EN           ( BIT(9) )
#define BIT_PRI_M8TOS3_ADJ_EN           ( BIT(8) )
#define BITS_PRI_M9TOS3_RND_THR(_x_)    ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M9TOS3_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M9TOS3_RND_EN           ( BIT(1) )
#define BIT_PRI_M9TOS3_ADJ_EN           ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S3_PRIO1 */
#define BITS_PRI_M2TOS3_RND_THR(_x_)    ( (_x_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M2TOS3_SEL(_x_)        ( (_x_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M2TOS3_RND_EN           ( BIT(25) )
#define BIT_PRI_M2TOS3_ADJ_EN           ( BIT(24) )
#define BITS_PRI_M3TOS3_RND_THR(_x_)    ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M3TOS3_SEL(_x_)        ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M3TOS3_RND_EN           ( BIT(17) )
#define BIT_PRI_M3TOS3_ADJ_EN           ( BIT(16) )
#define BITS_PRI_M4TOS3_RND_THR(_x_)    ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M4TOS3_SEL(_x_)        ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M4TOS3_RND_EN           ( BIT(9) )
#define BIT_PRI_M4TOS3_ADJ_EN           ( BIT(8) )
#define BITS_PRI_M5TOS3_RND_THR(_x_)    ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M5TOS3_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M5TOS3_RND_EN           ( BIT(1) )
#define BIT_PRI_M5TOS3_ADJ_EN           ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S3_PRIO2 */
#define BITS_PRI_M0TOS3_RND_THR(_x_)    ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M0TOS3_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M0TOS3_RND_EN           ( BIT(1) )
#define BIT_PRI_M0TOS3_ADJ_EN           ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S2_PRIO0 */
#define BITS_PRI_M0TOS2_RND_THR(_x_)    ( (_x_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M0TOS2_SEL(_x_)        ( (_x_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M0TOS2_RND_EN           ( BIT(25) )
#define BIT_PRI_M0TOS2_ADJ_EN           ( BIT(24) )
#define BITS_PRI_M1TOS2_RND_THR(_x_)    ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M1TOS2_SEL(_x_)        ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M1TOS2_RND_EN           ( BIT(17) )
#define BIT_PRI_M1TOS2_ADJ_EN           ( BIT(16) )
#define BITS_PRI_M2TOS2_RND_THR(_x_)    ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M2TOS2_SEL(_x_)        ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M2TOS2_RND_EN           ( BIT(9) )
#define BIT_PRI_M2TOS2_ADJ_EN           ( BIT(8) )
#define BITS_PRI_M3TOS2_RND_THR(_x_)    ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M3TOS2_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M3TOS2_RND_EN           ( BIT(1) )
#define BIT_PRI_M3TOS2_ADJ_EN           ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S1_PRIO0 */
#define BITS_PRI_M0TOS1_RND_THR(_x_)    ( (_x_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M0TOS1_SEL(_x_)        ( (_x_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M0TOS1_RND_EN           ( BIT(25) )
#define BIT_PRI_M0TOS1_ADJ_EN           ( BIT(24) )
#define BITS_PRI_M2TOS1_RND_THR(_x_)    ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M2TOS1_SEL(_x_)        ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M2TOS1_RND_EN           ( BIT(17) )
#define BIT_PRI_M2TOS1_ADJ_EN           ( BIT(16) )
#define BITS_PRI_M3TOS1_RND_THR(_x_)    ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M3TOS1_SEL(_x_)        ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M3TOS1_RND_EN           ( BIT(9) )
#define BIT_PRI_M3TOS1_ADJ_EN           ( BIT(8) )
#define BITS_PRI_M8TOS1_RND_THR(_x_)    ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M8TOS1_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M8TOS1_RND_EN           ( BIT(1) )
#define BIT_PRI_M8TOS1_ADJ_EN           ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S0_PRIO0 */
#define BITS_PRI_M0TOS0_RND_THR(_x_)    ( (_x_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M0TOS0_SEL(_x_)        ( (_x_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M0TOS0_RND_EN           ( BIT(25) )
#define BIT_PRI_M0TOS0_ADJ_EN           ( BIT(24) )
#define BITS_PRI_M1TOS0_RND_THR(_x_)    ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M1TOS0_SEL(_x_)        ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M1TOS0_RND_EN           ( BIT(17) )
#define BIT_PRI_M1TOS0_ADJ_EN           ( BIT(16) )
#define BITS_PRI_M2TOS0_RND_THR(_x_)    ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M2TOS0_SEL(_x_)        ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M2TOS0_RND_EN           ( BIT(9) )
#define BIT_PRI_M2TOS0_ADJ_EN           ( BIT(8) )
#define BITS_PRI_M3TOS0_RND_THR(_x_)    ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M3TOS0_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M3TOS0_RND_EN           ( BIT(1) )
#define BIT_PRI_M3TOS0_ADJ_EN           ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S0_PRIO1 */
#define BITS_PRI_M4TOS1_RND_THR(_x_)    ( (_x_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M4TOS1_SEL(_x_)        ( (_x_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M4TOS1_RND_EN           ( BIT(25) )
#define BIT_PRI_M4TOS1_ADJ_EN           ( BIT(24) )
#define BITS_PRI_M5TOS1_RND_THR(_x_)    ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M5TOS1_SEL(_x_)        ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M5TOS1_RND_EN           ( BIT(17) )
#define BIT_PRI_M5TOS1_ADJ_EN           ( BIT(16) )
#define BITS_PRI_M6TOS1_RND_THR(_x_)    ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M6TOS1_SEL(_x_)        ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M6TOS1_RND_EN           ( BIT(9) )
#define BIT_PRI_M6TOS1_ADJ_EN           ( BIT(8) )
#define BITS_PRI_M7TOS1_RND_THR(_x_)    ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M7TOS1_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M7TOS1_RND_EN           ( BIT(1) )
#define BIT_PRI_M7TOS1_ADJ_EN           ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S0_PRIO2 */
#define BITS_PRI_M9TOS1_RND_THR(_x_)    ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M9TOS1_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M9TOS1_RND_EN           ( BIT(1) )
#define BIT_PRI_M9TOS1_ADJ_EN           ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG */
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

/* bits definitions for register REG_AP_AHB_CHIP_ID */
#define BITS_CHIP_ID(_x_)               ( (_x_) << 0 )

/* vars definitions for controller REGS_AP_AHB */

#endif //__REGS_AP_AHB_H__
