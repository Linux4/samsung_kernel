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


#ifndef __H_REGS_CP_AHB_RF_HEADFILE_H__
#define __H_REGS_CP_AHB_RF_HEADFILE_H__ __FILE__

#define	REGS_CP_AHB_RF

/* registers definitions for CP_AHB_RF */
#define REG_CP_AHB_RF_AHB_EB0_STS                         SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0008)
#define REG_CP_AHB_RF_AHB_RST0_STS                        SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0014)
#define REG_CP_AHB_RF_ARCH_EB                             SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0018)
#define REG_CP_AHB_RF_AHB_REMAP                           SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x001C)
#define REG_CP_AHB_RF_AHB_MISC_CTL                        SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0020)
#define REG_CP_AHB_RF_AHB_MTX_PRI_SEL0                    SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0024)
#define REG_CP_AHB_RF_AHB_MTX_PRI_SEL1                    SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0028)
#define REG_CP_AHB_RF_AHB_MTX_PRI_SEL2                    SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x002C)
#define REG_CP_AHB_RF_AHB_MTX_PRI_SEL3                    SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0030)
#define REG_CP_AHB_RF_AHB_MTX_PRI_SEL4                    SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0034)
#define REG_CP_AHB_RF_AHB_MTX_PRI_SEL5                    SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0038)
#define REG_CP_AHB_RF_AHB_MTX_PRI_SEL6                    SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x003C)
#define REG_CP_AHB_RF_AHB_PAUSE                           SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0040)
#define REG_CP_AHB_RF_AHB_SLP_CTL                         SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0044)
#define REG_CP_AHB_RF_AHB_SLP_STS                         SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0048)
#define REG_CP_AHB_RF_MCU_CLK_CTL                         SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x004C)
#define REG_CP_AHB_RF_DSP_CTL                             SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0050)
#define REG_CP_AHB_RF_DSP_BOOT_VECTOR                     SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0054)
#define REG_CP_AHB_RF_DSP_RST                             SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0058)
#define REG_CP_AHB_RF_DSP_JTAG_CTL                        SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x005C)
#define REG_CP_AHB_RF_MCU_C2C_SEMA                        SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x0060)
#define REG_CP_AHB_RF_CHIP_ID                             SCI_ADDR(REGS_CP_AHB_RF_BASE, 0x01FC)



/* bits definitions for register REG_CP_AHB_RF_AHB_EB0_STS */
#define BIT_LZMA_EN                                       ( BIT(11) )
#define BIT_WCDMA_EN5                                     ( BIT(9) )
#define BIT_WCDMA_EN4                                     ( BIT(8) )
#define BIT_WCDMA_EN3                                     ( BIT(7) )
#define BIT_WCDMA_EN2                                     ( BIT(6) )
#define BIT_WCDMA_EN1                                     ( BIT(5) )
#define BIT_WCDMA_EN0                                     ( BIT(4) )
#define BIT_BUSMON2_EN                                    ( BIT(3) )
#define BIT_BUSMON1_EN                                    ( BIT(2) )
#define BIT_BUSMON0_EN                                    ( BIT(1) )
#define BIT_DMA_EN                                        ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_AHB_RST0_STS */
#define BIT_LZMA_SOFT_RST                                 ( BIT(8) )
#define BIT_WSYS_SOFT_RST                                 ( BIT(6) )
#define BIT_ARM2_SOFT_RST                                 ( BIT(5) )
#define BIT_ARM1_SOFT_RST                                 ( BIT(4) )
#define BIT_BUSMON2_SOFT_RST                              ( BIT(3) )
#define BIT_BUSMON1_SOFT_RST                              ( BIT(2) )
#define BIT_BUSMON0_SOFT_RST                              ( BIT(1) )
#define BIT_DMA_SOFT_RST                                  ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_ARCH_EB */
#define BIT_AHB_ARCH_EB                                   ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_AHB_REMAP */
#define BIT_PTEST_FUNC_ATSPEED_SEL                        ( BIT(2) )
#define BIT_PTEST_FUNC_MODE                               ( BIT(1) )
#define BIT_REMAP                                         ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_AHB_MISC_CTL */
#define BITS_DSP_M_PROT(_X_)                              ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_LZMA_M_PROT(_X_)                             ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_DMA_W_PROT(_X_)                              ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DMA_R_PROT(_X_)                              ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_MCU_SHM_CTRL(_X_)                            ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BIT_WCDMA_ASHB_EN                                 ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_AHB_MTX_PRI_SEL0 */
#define BITS_ARMMTX_M3TOS0_RND_THR(_X_)                   ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_ARMMTX_M3TOS0_ADJ_EN                          ( BIT(27) )
#define BIT_ARMMTX_M3TOS0_RND_EN                          ( BIT(26) )
#define BITS_ARMMTX_M3TOS0_SEL(_X_)                       ( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_ARMMTX_M2TOS0_RND_THR(_X_)                   ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BIT_ARMMTX_M2TOS0_ADJ_EN                          ( BIT(19) )
#define BIT_ARMMTX_M2TOS0_RND_EN                          ( BIT(18) )
#define BITS_ARMMTX_M2TOS0_SEL(_X_)                       ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_ARMMTX_M1TOS0_RND_THR(_X_)                   ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ARMMTX_M1TOS0_ADJ_EN                          ( BIT(11) )
#define BIT_ARMMTX_M1TOS0_RND_EN                          ( BIT(10) )
#define BITS_ARMMTX_M1TOS0_SEL(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARMMTX_M0TOS0_RND_THR(_X_)                   ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ARMMTX_M0TOS0_ADJ_EN                          ( BIT(3) )
#define BIT_ARMMTX_M0TOS0_RND_EN                          ( BIT(2) )
#define BITS_ARMMTX_M0TOS0_SEL(_X_)                       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CP_AHB_RF_AHB_MTX_PRI_SEL1 */
#define BITS_ARMMTX_M3TOS1_RND_THR(_X_)                   ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_ARMMTX_M3TOS1_ADJ_EN                          ( BIT(27) )
#define BIT_ARMMTX_M3TOS1_RND_EN                          ( BIT(26) )
#define BITS_ARMMTX_M3TOS1_SEL(_X_)                       ( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_ARMMTX_M2TOS1_RND_THR(_X_)                   ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BIT_ARMMTX_M2TOS1_ADJ_EN                          ( BIT(19) )
#define BIT_ARMMTX_M2TOS1_RND_EN                          ( BIT(18) )
#define BITS_ARMMTX_M2TOS1_SEL(_X_)                       ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_ARMMTX_M1TOS1_RND_THR(_X_)                   ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ARMMTX_M1TOS1_ADJ_EN                          ( BIT(11) )
#define BIT_ARMMTX_M1TOS1_RND_EN                          ( BIT(10) )
#define BITS_ARMMTX_M1TOS1_SEL(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARMMTX_M0TOS1_RND_THR(_X_)                   ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ARMMTX_M0TOS1_ADJ_EN                          ( BIT(3) )
#define BIT_ARMMTX_M0TOS1_RND_EN                          ( BIT(2) )
#define BITS_ARMMTX_M0TOS1_SEL(_X_)                       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CP_AHB_RF_AHB_MTX_PRI_SEL2 */
#define BITS_ARMMTX_M1TOS2_RND_THR(_X_)                   ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_ARMMTX_M1TOS2_ADJ_EN                          ( BIT(27) )
#define BIT_ARMMTX_M1TOS2_RND_EN                          ( BIT(26) )
#define BITS_ARMMTX_M1TOS2_SEL(_X_)                       ( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_ARMMTX_M0TOS2_RND_THR(_X_)                   ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BIT_ARMMTX_M0TOS2_ADJ_EN                          ( BIT(19) )
#define BIT_ARMMTX_M0TOS2_RND_EN                          ( BIT(18) )
#define BITS_ARMMTX_M0TOS2_SEL(_X_)                       ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_ARMMTX_M5TOS1_RND_THR(_X_)                   ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ARMMTX_M5TOS1_ADJ_EN                          ( BIT(11) )
#define BIT_ARMMTX_M5TOS1_RND_EN                          ( BIT(10) )
#define BITS_ARMMTX_M5TOS1_SEL(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARMMTX_M4TOS1_RND_THR(_X_)                   ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ARMMTX_M4TOS1_ADJ_EN                          ( BIT(3) )
#define BIT_ARMMTX_M4TOS1_RND_EN                          ( BIT(2) )
#define BITS_ARMMTX_M4TOS1_SEL(_X_)                       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CP_AHB_RF_AHB_MTX_PRI_SEL3 */
#define BITS_ARMMTX_M5TOS2_RND_THR(_X_)                   ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_ARMMTX_M5TOS2_ADJ_EN                          ( BIT(27) )
#define BIT_ARMMTX_M5TOS2_RND_EN                          ( BIT(26) )
#define BITS_ARMMTX_M5TOS2_SEL(_X_)                       ( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_ARMMTX_M4TOS2_RND_THR(_X_)                   ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BIT_ARMMTX_M4TOS2_ADJ_EN                          ( BIT(19) )
#define BIT_ARMMTX_M4TOS2_RND_EN                          ( BIT(18) )
#define BITS_ARMMTX_M4TOS2_SEL(_X_)                       ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_ARMMTX_M3TOS2_RND_THR(_X_)                   ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ARMMTX_M3TOS2_ADJ_EN                          ( BIT(11) )
#define BIT_ARMMTX_M3TOS2_RND_EN                          ( BIT(10) )
#define BITS_ARMMTX_M3TOS2_SEL(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARMMTX_M2TOS2_RND_THR(_X_)                   ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ARMMTX_M2TOS2_ADJ_EN                          ( BIT(3) )
#define BIT_ARMMTX_M2TOS2_RND_EN                          ( BIT(2) )
#define BITS_ARMMTX_M2TOS2_SEL(_X_)                       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CP_AHB_RF_AHB_MTX_PRI_SEL4 */
#define BITS_ARMMTX_M4TOS3_RND_THR(_X_)                   ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BIT_ARMMTX_M4TOS3_ADJ_EN                          ( BIT(19) )
#define BIT_ARMMTX_M4TOS3_RND_EN                          ( BIT(18) )
#define BITS_ARMMTX_M4TOS3_SEL(_X_)                       ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_ARMMTX_M1TOS3_RND_THR(_X_)                   ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ARMMTX_M1TOS3_ADJ_EN                          ( BIT(11) )
#define BIT_ARMMTX_M1TOS3_RND_EN                          ( BIT(10) )
#define BITS_ARMMTX_M1TOS3_SEL(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARMMTX_M0TOS3_RND_THR(_X_)                   ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ARMMTX_M0TOS3_ADJ_EN                          ( BIT(3) )
#define BIT_ARMMTX_M0TOS3_RND_EN                          ( BIT(2) )
#define BITS_ARMMTX_M0TOS3_SEL(_X_)                       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CP_AHB_RF_AHB_MTX_PRI_SEL5 */
#define BITS_ARMMTX_M4TOS0_RND_THR(_X_)                   ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_ARMMTX_M4TOS0_ADJ_EN                          ( BIT(27) )
#define BIT_ARMMTX_M4TOS0_RND_EN                          ( BIT(26) )
#define BITS_ARMMTX_M4TOS0_SEL(_X_)                       ( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_ARMMTX_M4TOS4_RND_THR(_X_)                   ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BIT_ARMMTX_M4TOS4_ADJ_EN                          ( BIT(19) )
#define BIT_ARMMTX_M4TOS4_RND_EN                          ( BIT(18) )
#define BITS_ARMMTX_M4TOS4_SEL(_X_)                       ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_ARMMTX_M1TOS4_RND_THR(_X_)                   ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ARMMTX_M1TOS4_ADJ_EN                          ( BIT(11) )
#define BIT_ARMMTX_M1TOS4_RND_EN                          ( BIT(10) )
#define BITS_ARMMTX_M1TOS4_SEL(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARMMTX_M0TOS4_RND_THR(_X_)                   ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ARMMTX_M0TOS4_ADJ_EN                          ( BIT(3) )
#define BIT_ARMMTX_M0TOS4_RND_EN                          ( BIT(2) )
#define BITS_ARMMTX_M0TOS4_SEL(_X_)                       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CP_AHB_RF_AHB_MTX_PRI_SEL6 */
#define BITS_ARMMTX_M3TOS5_RND_THR(_X_)                   ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_ARMMTX_M3TOS5_ADJ_EN                          ( BIT(27) )
#define BIT_ARMMTX_M3TOS5_RND_EN                          ( BIT(26) )
#define BITS_ARMMTX_M3TOS5_SEL(_X_)                       ( (_X_) << 24 & (BIT(24)|BIT(25)) )
#define BITS_ARMMTX_M2TOS5_RND_THR(_X_)                   ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BIT_ARMMTX_M2TOS5_ADJ_EN                          ( BIT(19) )
#define BIT_ARMMTX_M2TOS5_RND_EN                          ( BIT(18) )
#define BITS_ARMMTX_M2TOS5_SEL(_X_)                       ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_ARMMTX_M1TOS5_RND_THR(_X_)                   ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ARMMTX_M1TOS5_ADJ_EN                          ( BIT(11) )
#define BIT_ARMMTX_M1TOS5_RND_EN                          ( BIT(10) )
#define BITS_ARMMTX_M1TOS5_SEL(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARMMTX_M0TOS5_RND_THR(_X_)                   ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ARMMTX_M0TOS5_ADJ_EN                          ( BIT(3) )
#define BIT_ARMMTX_M0TOS5_RND_EN                          ( BIT(2) )
#define BITS_ARMMTX_M0TOS5_SEL(_X_)                       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CP_AHB_RF_AHB_PAUSE */
#define BIT_MCU_LIGHT_SLEEP_EN                            ( BIT(4) )
#define BIT_APB_SLEEP                                     ( BIT(3) )
#define BIT_MCU_DEEP_SLEEP_EN                             ( BIT(2) )
#define BIT_MCU_SYS_SLEEP_EN                              ( BIT(1) )
#define BIT_MCU_CORE_SLEEP                                ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_AHB_SLP_CTL */
#define BIT_ARM_DAHB_SLEEP_EN                             ( BIT(4) )
#define BIT_MCUMTX_AUTO_GATE_EN                           ( BIT(3) )
#define BIT_MCU_AUTO_GATE_EN                              ( BIT(2) )
#define BIT_AHB_AUTO_GATE_EN                              ( BIT(1) )
#define BIT_ARM_AUTO_GATE_EN                              ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_AHB_SLP_STS */
#define BIT_APB_PERI_SLEEP                                ( BIT(3) )
#define BIT_DSP_MAHB_SLEEP_EN                             ( BIT(2) )
#define BIT_DMA_BUSY                                      ( BIT(1) )
#define BIT_EMC_SLEEP                                     ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_MCU_CLK_CTL */
#define BITS_CLK_LZMA_SEL(_X_)                            ( (_X_) << 25 & (BIT(25)|BIT(26)) )
#define BIT_CLK_CP_DBG_SEL                                ( BIT(21) )
#define BIT_CLK_MCU_DBG_SEL                               ( BIT(20) )
#define BITS_CLK_COM_DBG_SEL(_X_)                         ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_CLK_SYS_DBG_SEL(_X_)                         ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_CLK_AHB_DIV(_X_)                             ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)) )
#define BITS_CLK_MCU_DIV(_X_)                             ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_CLK_APB_SEL(_X_)                             ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_CLK_MCU_SEL(_X_)                             ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_CP_AHB_RF_DSP_CTL */
#define BIT_ASHB_ARMTODSP_EN                              ( BIT(2) )
#define BIT_FRC_CLK_DSP_EN                                ( BIT(1) )
#define BIT_DSP_BOOT_EN                                   ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_DSP_BOOT_VECTOR */
#define BITS_DSP_BOOT_VECTOR(_X_)                         (_X_)

/* bits definitions for register REG_CP_AHB_RF_DSP_RST */
#define BIT_DSP_SYS_SRST                                  ( BIT(1) )
#define BIT_DSP_CORE_SRST_N                               ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_DSP_JTAG_CTL */
#define BIT_CEVA_SW_JTAG_ENA                              ( BIT(8) )
#define BIT_STDO                                          ( BIT(4) )
#define BIT_STCK                                          ( BIT(3) )
#define BIT_STMS                                          ( BIT(2) )
#define BIT_STDI                                          ( BIT(1) )
#define BIT_STRTCK                                        ( BIT(0) )

/* bits definitions for register REG_CP_AHB_RF_MCU_C2C_SEMA */
#define BITS_MCU_C2C_SEMA(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_CP_AHB_RF_CHIP_ID */
#define BITS_CHIP_ID(_X_)                                 (_X_)

#endif
