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

#define	REGS_AP_AHB

/* registers definitions for AP_AHB */
#define REG_AP_AHB_AHB_EB                                 SCI_ADDR(REGS_AP_AHB_BASE, 0x0000)
#define REG_AP_AHB_AHB_RST                                SCI_ADDR(REGS_AP_AHB_BASE, 0x0004)
#define REG_AP_AHB_CA7_RST_SET                            SCI_ADDR(REGS_AP_AHB_BASE, 0x0008)
#define REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG                 SCI_ADDR(REGS_AP_AHB_BASE, 0x000C)
#define REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG                  SCI_ADDR(REGS_AP_AHB_BASE, 0x0010)
#define REG_AP_AHB_HOLDING_PEN                            SCI_ADDR(REGS_AP_AHB_BASE, 0x0014)
#define REG_AP_AHB_JMP_ADDR_CA7_C0                        SCI_ADDR(REGS_AP_AHB_BASE, 0x0018)
#define REG_AP_AHB_JMP_ADDR_CA7_C1                        SCI_ADDR(REGS_AP_AHB_BASE, 0x001C)
#define REG_AP_AHB_JMP_ADDR_CA7_C2                        SCI_ADDR(REGS_AP_AHB_BASE, 0x0020)
#define REG_AP_AHB_JMP_ADDR_CA7_C3                        SCI_ADDR(REGS_AP_AHB_BASE, 0x0024)
#define REG_AP_AHB_CA7_C0_PU_LOCK                         SCI_ADDR(REGS_AP_AHB_BASE, 0x0028)
#define REG_AP_AHB_CA7_C1_PU_LOCK                         SCI_ADDR(REGS_AP_AHB_BASE, 0x002C)
#define REG_AP_AHB_CA7_C2_PU_LOCK                         SCI_ADDR(REGS_AP_AHB_BASE, 0x0030)
#define REG_AP_AHB_CA7_C3_PU_LOCK                         SCI_ADDR(REGS_AP_AHB_BASE, 0x0034)
#define REG_AP_AHB_CA7_CKG_CFG                            SCI_ADDR(REGS_AP_AHB_BASE, 0x3000)
#define REG_AP_AHB_MCU_PAUSE                              SCI_ADDR(REGS_AP_AHB_BASE, 0x3004)
#define REG_AP_AHB_MISC_CKG_EN                            SCI_ADDR(REGS_AP_AHB_BASE, 0x3008)
#define REG_AP_AHB_MISC_CFG                               SCI_ADDR(REGS_AP_AHB_BASE, 0x300C)
#define REG_AP_AHB_AP_MTX_S3_PRIO0                        SCI_ADDR(REGS_AP_AHB_BASE, 0x3010)
#define REG_AP_AHB_AP_MTX_S3_PRIO1                        SCI_ADDR(REGS_AP_AHB_BASE, 0x3014)
#define REG_AP_AHB_AP_MTX_S3_PRIO2                        SCI_ADDR(REGS_AP_AHB_BASE, 0x3018)
#define REG_AP_AHB_AP_MTX_S2_PRIO0                        SCI_ADDR(REGS_AP_AHB_BASE, 0x301C)
#define REG_AP_AHB_AP_MTX_S1_PRIO0                        SCI_ADDR(REGS_AP_AHB_BASE, 0x3020)
#define REG_AP_AHB_AP_MTX_S0_PRIO0                        SCI_ADDR(REGS_AP_AHB_BASE, 0x3024)
#define REG_AP_AHB_AP_MTX_S0_PRIO1                        SCI_ADDR(REGS_AP_AHB_BASE, 0x3028)
#define REG_AP_AHB_AP_MTX_S0_PRIO2                        SCI_ADDR(REGS_AP_AHB_BASE, 0x302C)
#define REG_AP_AHB_AP_MAIN_MTX_HPROT_CFG                  SCI_ADDR(REGS_AP_AHB_BASE, 0x3030)
#define REG_AP_AHB_CA7_STANDBY_STATUS                     SCI_ADDR(REGS_AP_AHB_BASE, 0x3034)
#define REG_AP_AHB_NANC_CLK_CFG                           SCI_ADDR(REGS_AP_AHB_BASE, 0x3038)
#define REG_AP_AHB_LVDS_CFG                               SCI_ADDR(REGS_AP_AHB_BASE, 0x303C)
#define REG_AP_AHB_LVDS_PLL_CFG0                          SCI_ADDR(REGS_AP_AHB_BASE, 0x3040)
#define REG_AP_AHB_LVDS_PLL_CFG1                          SCI_ADDR(REGS_AP_AHB_BASE, 0x3044)
#define REG_AP_AHB_CA7_C0_AUTO_FORCE_SHUTDOWN_EN          SCI_ADDR(REGS_AP_AHB_BASE, 0x3048)
#define REG_AP_AHB_CA7_C1_AUTO_FORCE_SHUTDOWN_EN          SCI_ADDR(REGS_AP_AHB_BASE, 0x304C)
#define REG_AP_AHB_CA7_C2_AUTO_FORCE_SHUTDOWN_EN          SCI_ADDR(REGS_AP_AHB_BASE, 0x3050)
#define REG_AP_AHB_CA7_C3_AUTO_FORCE_SHUTDOWN_EN          SCI_ADDR(REGS_AP_AHB_BASE, 0x3054)
#define REG_AP_AHB_AP_QOS_CFG                             SCI_ADDR(REGS_AP_AHB_BASE, 0x3058)
#define REG_AP_AHB_CHIP_ID                                SCI_ADDR(REGS_AP_AHB_BASE, 0x30FC)



/* bits definitions for register REG_AP_AHB_AHB_EB */
#define BIT_LVDS_EB                                       ( BIT(22) )
#define BIT_ZIPDEC_EB                                     ( BIT(21) )
#define BIT_ZIPENC_EB                                     ( BIT(20) )
#define BIT_NANDC_ECC_EB                                  ( BIT(19) )
#define BIT_NANDC_2X_EB                                   ( BIT(18) )
#define BIT_NANDC_EB                                      ( BIT(17) )
#define BIT_BUSMON2_EB                                    ( BIT(16) )
#define BIT_BUSMON1_EB                                    ( BIT(15) )
#define BIT_BUSMON0_EB                                    ( BIT(14) )
#define BIT_SPINLOCK_EB                                   ( BIT(13) )
#define BIT_GPS_EB                                        ( BIT(12) )
#define BIT_EMMC_EB                                       ( BIT(11) )
#define BIT_SDIO2_EB                                      ( BIT(10) )
#define BIT_SDIO1_EB                                      ( BIT(9) )
#define BIT_SDIO0_EB                                      ( BIT(8) )
#define BIT_DRM_EB                                        ( BIT(7) )
#define BIT_NFC_EB                                        ( BIT(6) )
#define BIT_DMA_EB                                        ( BIT(5) )
#define BIT_USB_EB                                        ( BIT(4) )
#define BIT_GSP_EB                                        ( BIT(3) )
#define BIT_DISPC1_EB                                     ( BIT(2) )
#define BIT_DISPC0_EB                                     ( BIT(1) )
#define BIT_DSI_EB                                        ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AHB_RST */
#define BIT_LVDS_SOFT_RST                                 ( BIT(25) )
#define BIT_ZIP_MTX_SOFT_RST                              ( BIT(24) )
#define BIT_ZIPDEC_SOFT_RST                               ( BIT(23) )
#define BIT_ZIPENC_SOFT_RST                               ( BIT(22) )
#define BIT_NANDC_SOFT_RST                                ( BIT(20) )
#define BIT_BUSMON2_SOFT_RST                              ( BIT(19) )
#define BIT_BUSMON1_SOFT_RST                              ( BIT(18) )
#define BIT_BUSMON0_SOFT_RST                              ( BIT(17) )
#define BIT_SPINLOCK_SOFT_RST                             ( BIT(16) )
#define BIT_GPS_SOFT_RST                                  ( BIT(15) )
#define BIT_EMMC_SOFT_RST                                 ( BIT(14) )
#define BIT_SDIO2_SOFT_RST                                ( BIT(13) )
#define BIT_SDIO1_SOFT_RST                                ( BIT(12) )
#define BIT_SDIO0_SOFT_RST                                ( BIT(11) )
#define BIT_DRM_SOFT_RST                                  ( BIT(10) )
#define BIT_NFC_SOFT_RST                                  ( BIT(9) )
#define BIT_DMA_SOFT_RST                                  ( BIT(8) )
#define BIT_USB_PHY_SOFT_RST                              ( BIT(7) )
#define BIT_USB_UTMI_SOFT_RST                             ( BIT(6) )
#define BIT_USB_SOFT_RST                                  ( BIT(5) )
#define BIT_DISP_MTX_SOFT_RST                             ( BIT(4) )
#define BIT_GSP_SOFT_RST                                  ( BIT(3) )
#define BIT_DISPC1_SOFT_RST                               ( BIT(2) )
#define BIT_DISPC0_SOFT_RST                               ( BIT(1) )
#define BIT_DSI_SOFT_RST                                  ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_RST_SET */
#define BIT_CA7_CS_DBG_SOFT_RST                           ( BIT(14) )
#define BIT_CA7_L2_SOFT_RST                               ( BIT(13) )
#define BIT_CA7_SOCDBG_SOFT_RST                           ( BIT(12) )
#define BITS_CA7_ETM_SOFT_RST(_X_)                        ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CA7_DBG_SOFT_RST(_X_)                        ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CA7_CORE_SOFT_RST(_X_)                       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG */
#define BIT_AP_PERI_FORCE_ON                              ( BIT(2) )
#define BIT_AP_PERI_FORCE_SLP                             ( BIT(1) )
#define BIT_AP_APB_SLEEP                                  ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG */
#define BIT_GSP_CKG_FORCE_EN                              ( BIT(9) )
#define BIT_GSP_AUTO_GATE_EN                              ( BIT(8) )
#define BIT_AP_AHB_AUTO_GATE_EN                           ( BIT(5) )
#define BIT_AP_EMC_AUTO_GATE_EN                           ( BIT(4) )
#define BIT_CA7_EMC_AUTO_GATE_EN                          ( BIT(3) )
#define BIT_CA7_DBG_FORCE_SLEEP                           ( BIT(2) )
#define BIT_CA7_DBG_AUTO_GATE_EN                          ( BIT(1) )
#define BIT_CA7_CORE_AUTO_GATE_EN                         ( BIT(0) )

/* bits definitions for register REG_AP_AHB_HOLDING_PEN */
#define BITS_HOLDING_PEN(_X_)                             (_X_)

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C0 */
#define BITS_JMP_ADDR_CA7_C0(_X_)                         (_X_)

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C1 */
#define BITS_JMP_ADDR_CA7_C1(_X_)                         (_X_)

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C2 */
#define BITS_JMP_ADDR_CA7_C2(_X_)                         (_X_)

/* bits definitions for register REG_AP_AHB_JMP_ADDR_CA7_C3 */
#define BITS_JMP_ADDR_CA7_C3(_X_)                         (_X_)

/* bits definitions for register REG_AP_AHB_CA7_C0_PU_LOCK */
#define BIT_CA7_C0_PU_LOCK                                ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C1_PU_LOCK */
#define BIT_CA7_C1_PU_LOCK                                ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C2_PU_LOCK */
#define BIT_CA7_C2_PU_LOCK                                ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C3_PU_LOCK */
#define BIT_CA7_C3_PU_LOCK                                ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_CKG_CFG */
#define BITS_CA7_DBG_CKG_DIV(_X_)                         ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)) )
#define BITS_CA7_AXI_CKG_DIV(_X_)                         ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_CA7_MCU_CKG_DIV(_X_)                         ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_CA7_MCU_CKG_SEL(_X_)                         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_AHB_MCU_PAUSE */
#define BIT_DMA_ACT_LIGHT_EN                              ( BIT(5) )
#define BIT_MCU_SLEEP_FOLLOW_CA7_EN                       ( BIT(4) )
#define BIT_MCU_LIGHT_SLEEP_EN                            ( BIT(3) )
#define BIT_MCU_DEEP_SLEEP_EN                             ( BIT(2) )
#define BIT_MCU_SYS_SLEEP_EN                              ( BIT(1) )
#define BIT_MCU_CORE_SLEEP                                ( BIT(0) )

/* bits definitions for register REG_AP_AHB_MISC_CKG_EN */
#define BIT_GPS_TCXO_INV_SEL                              ( BIT(13) )
#define BIT_GPS_26M_INV_SEL                               ( BIT(12) )
#define BIT_ASHB_CA7_DBG_VLD                              ( BIT(9) )
#define BIT_ASHB_CA7_DBG_EN                               ( BIT(8) )
#define BIT_DISP_TMC_CKG_EN                               ( BIT(4) )
#define BIT_DPHY_REF_CKG_EN                               ( BIT(1) )
#define BIT_DPHY_CFG_CKG_EN                               ( BIT(0) )

/* bits definitions for register REG_AP_AHB_MISC_CFG */
#define BITS_EMMC_SLOT_SEL(_X_)                           ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_SDIO0_SLOT_SEL(_X_)                          ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_BUSMON2_CHN_SEL(_X_)                         ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_BUSMON1_CHN_SEL(_X_)                         ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BIT_BUSMON0_CHN_SEL                               ( BIT(4) )
#define BITS_SDIO2_SLOT_SEL(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_SDIO1_SLOT_SEL(_X_)                          ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S3_PRIO0 */
#define BITS_PRI_M6TOS3_RND_THR(_X_)                      ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M6TOS3_SEL(_X_)                          ( (_X_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M6TOS3_RND_EN                             ( BIT(25) )
#define BIT_PRI_M6TOS3_ADJ_EN                             ( BIT(24) )
#define BITS_PRI_M7TOS3_RND_THR(_X_)                      ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M7TOS3_SEL(_X_)                          ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M7TOS3_RND_EN                             ( BIT(17) )
#define BIT_PRI_M7TOS3_ADJ_EN                             ( BIT(16) )
#define BITS_PRI_M8TOS3_RND_THR(_X_)                      ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M8TOS3_SEL(_X_)                          ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M8TOS3_RND_EN                             ( BIT(9) )
#define BIT_PRI_M8TOS3_ADJ_EN                             ( BIT(8) )
#define BITS_PRI_M9TOS3_RND_THR(_X_)                      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M9TOS3_SEL(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M9TOS3_RND_EN                             ( BIT(1) )
#define BIT_PRI_M9TOS3_ADJ_EN                             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S3_PRIO1 */
#define BITS_PRI_M2TOS3_RND_THR(_X_)                      ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M2TOS3_SEL(_X_)                          ( (_X_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M2TOS3_RND_EN                             ( BIT(25) )
#define BIT_PRI_M2TOS3_ADJ_EN                             ( BIT(24) )
#define BITS_PRI_M3TOS3_RND_THR(_X_)                      ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M3TOS3_SEL(_X_)                          ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M3TOS3_RND_EN                             ( BIT(17) )
#define BIT_PRI_M3TOS3_ADJ_EN                             ( BIT(16) )
#define BITS_PRI_M4TOS3_RND_THR(_X_)                      ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M4TOS3_SEL(_X_)                          ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M4TOS3_RND_EN                             ( BIT(9) )
#define BIT_PRI_M4TOS3_ADJ_EN                             ( BIT(8) )
#define BITS_PRI_M5TOS3_RND_THR(_X_)                      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M5TOS3_SEL(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M5TOS3_RND_EN                             ( BIT(1) )
#define BIT_PRI_M5TOS3_ADJ_EN                             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S3_PRIO2 */
#define BITS_PRI_M0TOS3_RND_THR(_X_)                      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M0TOS3_SEL(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M0TOS3_RND_EN                             ( BIT(1) )
#define BIT_PRI_M0TOS3_ADJ_EN                             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S2_PRIO0 */
#define BITS_PRI_M0TOS2_RND_THR(_X_)                      ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M0TOS2_SEL(_X_)                          ( (_X_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M0TOS2_RND_EN                             ( BIT(25) )
#define BIT_PRI_M0TOS2_ADJ_EN                             ( BIT(24) )
#define BITS_PRI_M1TOS2_RND_THR(_X_)                      ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M1TOS2_SEL(_X_)                          ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M1TOS2_RND_EN                             ( BIT(17) )
#define BIT_PRI_M1TOS2_ADJ_EN                             ( BIT(16) )
#define BITS_PRI_M2TOS2_RND_THR(_X_)                      ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M2TOS2_SEL(_X_)                          ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M2TOS2_RND_EN                             ( BIT(9) )
#define BIT_PRI_M2TOS2_ADJ_EN                             ( BIT(8) )
#define BITS_PRI_M3TOS2_RND_THR(_X_)                      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M3TOS2_SEL(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M3TOS2_RND_EN                             ( BIT(1) )
#define BIT_PRI_M3TOS2_ADJ_EN                             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S1_PRIO0 */
#define BITS_PRI_M0TOS1_RND_THR(_X_)                      ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M0TOS1_SEL(_X_)                          ( (_X_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M0TOS1_RND_EN                             ( BIT(25) )
#define BIT_PRI_M0TOS1_ADJ_EN                             ( BIT(24) )
#define BITS_PRI_M2TOS1_RND_THR(_X_)                      ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M2TOS1_SEL(_X_)                          ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M2TOS1_RND_EN                             ( BIT(17) )
#define BIT_PRI_M2TOS1_ADJ_EN                             ( BIT(16) )
#define BITS_PRI_M3TOS1_RND_THR(_X_)                      ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M3TOS1_SEL(_X_)                          ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M3TOS1_RND_EN                             ( BIT(9) )
#define BIT_PRI_M3TOS1_ADJ_EN                             ( BIT(8) )
#define BITS_PRI_M8TOS1_RND_THR(_X_)                      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M8TOS1_SEL(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M8TOS1_RND_EN                             ( BIT(1) )
#define BIT_PRI_M8TOS1_ADJ_EN                             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S0_PRIO0 */
#define BITS_PRI_M0TOS0_RND_THR(_X_)                      ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M0TOS0_SEL(_X_)                          ( (_X_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M0TOS0_RND_EN                             ( BIT(25) )
#define BIT_PRI_M0TOS0_ADJ_EN                             ( BIT(24) )
#define BITS_PRI_M1TOS0_RND_THR(_X_)                      ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M1TOS0_SEL(_X_)                          ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M1TOS0_RND_EN                             ( BIT(17) )
#define BIT_PRI_M1TOS0_ADJ_EN                             ( BIT(16) )
#define BITS_PRI_M2TOS0_RND_THR(_X_)                      ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M2TOS0_SEL(_X_)                          ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M2TOS0_RND_EN                             ( BIT(9) )
#define BIT_PRI_M2TOS0_ADJ_EN                             ( BIT(8) )
#define BITS_PRI_M3TOS0_RND_THR(_X_)                      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M3TOS0_SEL(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M3TOS0_RND_EN                             ( BIT(1) )
#define BIT_PRI_M3TOS0_ADJ_EN                             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S0_PRIO1 */
#define BITS_PRI_M4TOS1_RND_THR(_X_)                      ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PRI_M4TOS1_SEL(_X_)                          ( (_X_) << 26 & (BIT(26)|BIT(27)) )
#define BIT_PRI_M4TOS1_RND_EN                             ( BIT(25) )
#define BIT_PRI_M4TOS1_ADJ_EN                             ( BIT(24) )
#define BITS_PRI_M5TOS1_RND_THR(_X_)                      ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PRI_M5TOS1_SEL(_X_)                          ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BIT_PRI_M5TOS1_RND_EN                             ( BIT(17) )
#define BIT_PRI_M5TOS1_ADJ_EN                             ( BIT(16) )
#define BITS_PRI_M6TOS1_RND_THR(_X_)                      ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PRI_M6TOS1_SEL(_X_)                          ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_PRI_M6TOS1_RND_EN                             ( BIT(9) )
#define BIT_PRI_M6TOS1_ADJ_EN                             ( BIT(8) )
#define BITS_PRI_M7TOS1_RND_THR(_X_)                      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M7TOS1_SEL(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M7TOS1_RND_EN                             ( BIT(1) )
#define BIT_PRI_M7TOS1_ADJ_EN                             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MTX_S0_PRIO2 */
#define BITS_PRI_M9TOS1_RND_THR(_X_)                      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PRI_M9TOS1_SEL(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BIT_PRI_M9TOS1_RND_EN                             ( BIT(1) )
#define BIT_PRI_M9TOS1_ADJ_EN                             ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_MAIN_MTX_HPROT_CFG */
#define BITS_HPROT_NFC(_X_)                               ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_HPROT_EMMC(_X_)                              ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_HPROT_SDIO2(_X_)                             ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_HPROT_SDIO1(_X_)                             ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HPROT_SDIO0(_X_)                             ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_HPROT_DMAW(_X_)                              ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_HPROT_DMAR(_X_)                              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_CA7_STANDBY_STATUS */
#define BIT_CA7_STANDBYWFIL2                              ( BIT(12) )
#define BITS_CA7_ETMSTANDBYWFX(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CA7_STANDBYWFE(_X_)                          ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CA7_STANDBYWFI(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_NANC_CLK_CFG */
#define BITS_CLK_NANDC2X_DIV(_X_)                         ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_CLK_NANDC2X_SEL(_X_)                         ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AP_AHB_LVDS_CFG */
#define BITS_LVDS_TXCLKDATA(_X_)                          ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)) )
#define BITS_LVDS_TXCOM(_X_)                              ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_LVDS_TXSLEW(_X_)                             ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_LVDS_TXSW(_X_)                               ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_LVDS_TXRERSER(_X_)                           ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LVDS_PRE_EMP(_X_)                            ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_LVDS_TXPD                                     ( BIT(0) )

/* bits definitions for register REG_AP_AHB_LVDS_PLL_CFG0 */
#define BIT_LPLL_LOCK_DET                                 ( BIT(31) )
#define BITS_LPLL_REFIN(_X_)                              ( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_LPLL_LPF(_X_)                                ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BIT_LPLL_DIV_S                                    ( BIT(18) )
#define BITS_LPLL_IBIAS(_X_)                              ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_LPLLN(_X_)                                   ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )

/* bits definitions for register REG_AP_AHB_LVDS_PLL_CFG1 */
#define BITS_LPLL_KINT(_X_)                               ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_LPLL_RSV(_X_)                                ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_LPLL_MOD_EN                                   ( BIT(7) )
#define BIT_LPLL_SDM_EN                                   ( BIT(6) )
#define BITS_LPLL_NINT(_X_)                               ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_AP_AHB_CA7_C0_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_CA7_C0_AUTO_FORCE_SHUTDOWN_EN                 ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C1_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_CA7_C1_AUTO_FORCE_SHUTDOWN_EN                 ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C2_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_CA7_C2_AUTO_FORCE_SHUTDOWN_EN                 ( BIT(0) )

/* bits definitions for register REG_AP_AHB_CA7_C3_AUTO_FORCE_SHUTDOWN_EN */
#define BIT_CA7_C3_AUTO_FORCE_SHUTDOWN_EN                 ( BIT(0) )

/* bits definitions for register REG_AP_AHB_AP_QOS_CFG */
#define BITS_QOS_R_TMC(_X_)                               ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_QOS_W_TMC(_X_)                               ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_QOS_R_DISPC1(_X_)                            ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_QOS_W_DISPC1(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_QOS_R_DISPC0(_X_)                            ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_QOS_W_DISPC0(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AP_AHB_CHIP_ID */
#define BITS_CHIP_ID(_X_)                                 (_X_)

#endif
