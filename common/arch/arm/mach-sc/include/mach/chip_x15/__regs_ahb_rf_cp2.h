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


#ifndef __H_REGS_AHB_RF_CP2_HEADFILE_H__
#define __H_REGS_AHB_RF_CP2_HEADFILE_H__ __FILE__

#define	REGS_AHB_RF_CP2

/* registers definitions for AHB_RF_CP2 */
#define REG_AHB_RF_CP2_AHB_RST0                           SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0000)
#define REG_AHB_RF_CP2_AHB_EB0                            SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0004)
#define REG_AHB_RF_CP2_ARCH_EB_REG                        SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0084)
#define REG_AHB_RF_CP2_AHB_BUS_CTL0                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x00A0)
#define REG_AHB_RF_CP2_AHB_BUS_CTL1                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x00A4)
#define REG_AHB_RF_CP2_AHB_BUS_CTL2                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x00A8)
#define REG_AHB_RF_CP2_AHB_SYS_CTL0                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x00B0)
#define REG_AHB_RF_CP2_AHB_SYS_CTL3                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x00BC)
#define REG_AHB_RF_CP2_AHB_SLP_CTL0                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x00D0)
#define REG_AHB_RF_CP2_AHB_SLP_CTL1                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x00D4)
#define REG_AHB_RF_CP2_AHB_SLP_CTL2                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x00D8)
#define REG_AHB_RF_CP2_AHB_SYS_CTL6                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x00E0)
#define REG_AHB_RF_CP2_AHB_SLP_STS0                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0100)
#define REG_AHB_RF_CP2_AHB_MTX_CTL0                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0110)
#define REG_AHB_RF_CP2_AHB_MTX_CTL1                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0114)
#define REG_AHB_RF_CP2_AHB_MTX_CTL2                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0118)
#define REG_AHB_RF_CP2_AHB_MTX_CTL3                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x011C)
#define REG_AHB_RF_CP2_AHB_MTX_CTL4                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0120)
#define REG_AHB_RF_CP2_AHB_MTX_CTL5                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0124)
#define REG_AHB_RF_CP2_AHB_MTX_CTL6                       SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0128)
#define REG_AHB_RF_CP2_AHB_ARCH_PORT                      SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x0130)
#define REG_AHB_RF_CP2_AHB_CHIP_ID                        SCI_ADDR(REGS_AHB_RF_CP2_BASE, 0x03FC)



/* bits definitions for register REG_AHB_RF_CP2_AHB_RST0 */
#define BIT_VPU_SOFT_RST                                  ( BIT(3) )
#define BIT_RFSBI_SOFT_RST                                ( BIT(2) )
#define BIT_BUSM_SOFT_RST                                 ( BIT(1) )
#define BIT_DMA_SOFT_RST                                  ( BIT(0) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_EB0 */
#define BIT_BUSMON1_EB                                    ( BIT(3) )
#define BIT_BUSMON0_EB                                    ( BIT(2) )
#define BIT_RFSBI_EB                                      ( BIT(1) )
#define BIT_DMA_EB                                        ( BIT(0) )

/* bits definitions for register REG_AHB_RF_CP2_ARCH_EB_REG */
#define BIT_ARCH_EB                                       ( BIT(0) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_BUS_CTL0 */
#define BIT_M5_PAUSE_HW0_EN                               ( BIT(5) )
#define BIT_M4_PAUSE_HW0_EN                               ( BIT(4) )
#define BIT_M3_PAUSE_HW0_EN                               ( BIT(3) )
#define BIT_M2_PAUSE_HW0_EN                               ( BIT(2) )
#define BIT_M1_PAUSE_HW0_EN                               ( BIT(1) )
#define BIT_M0_PAUSE_HW0_EN                               ( BIT(0) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_BUS_CTL1 */
#define BIT_M5_PAUSE_HW1_EN                               ( BIT(5) )
#define BIT_M4_PAUSE_HW1_EN                               ( BIT(4) )
#define BIT_M3_PAUSE_HW1_EN                               ( BIT(3) )
#define BIT_M2_PAUSE_HW1_EN                               ( BIT(2) )
#define BIT_M1_PAUSE_HW1_EN                               ( BIT(1) )
#define BIT_M0_PAUSE_HW1_EN                               ( BIT(0) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_BUS_CTL2 */
#define BIT_M5_PAUSE_SW                                   ( BIT(5) )
#define BIT_M4_PAUSE_SW                                   ( BIT(4) )
#define BIT_M3_PAUSE_SW                                   ( BIT(3) )
#define BIT_M2_PAUSE_SW                                   ( BIT(2) )
#define BIT_M1_PAUSE_SW                                   ( BIT(1) )
#define BIT_M0_PAUSE_SW                                   ( BIT(0) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_SYS_CTL0 */
#define BITS_WIFI_MON_OUT(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)) )
#define BITS_BUSMON1_CHN_SEL(_X_)                         ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_BUSMON0_CHN_SEL(_X_)                         ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_WIFI_MON_SEL(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_SYS_CTL3 */
#define BITS_DMA_REQ_HW_SEL(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_SLP_CTL0 */
#define BIT_APB_STOP                                      ( BIT(5) )
#define BIT_MCU_CORE_SLP                                  ( BIT(1) )
#define BIT_MCU_FORCE_SYS_SLP                             ( BIT(0) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_SLP_CTL1 */
#define BIT_MCU_LIGHT_SLEEP_EN                            ( BIT(8) )
#define BIT_DMA_ACT_LIGHT_EN                              ( BIT(7) )
#define BIT_DMA_SLP_MODE                                  ( BIT(6) )
#define BIT_MCU_DEEP_SLP_EN                               ( BIT(4) )
#define BIT_MCU_SYS_SLP_EN                                ( BIT(3) )
#define BIT_MCU_DMA_WAKE_UP_EN                            ( BIT(2) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_SLP_CTL2 */
#define BITS_DMA_REQ_WAKE_EN(_X_)                         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_SYS_CTL6 */
#define BIT_PTEST                                         ( BIT(0) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_SLP_STS0 */
#define BIT_MTX_M5_STOP                                   ( BIT(5) )
#define BIT_MTX_M4_STOP                                   ( BIT(4) )
#define BIT_MTX_M3_STOP                                   ( BIT(3) )
#define BIT_MTX_M2_STOP                                   ( BIT(2) )
#define BIT_MTX_M1_STOP                                   ( BIT(1) )
#define BIT_MTX_M0_STOP                                   ( BIT(0) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_MTX_CTL0 */
#define BITS_MATRIX_CTRL0(_X_)                            (_X_)

/* bits definitions for register REG_AHB_RF_CP2_AHB_MTX_CTL1 */
#define BITS_MATRIX_CTRL1(_X_)                            (_X_)

/* bits definitions for register REG_AHB_RF_CP2_AHB_MTX_CTL2 */
#define BITS_MATRIX_CTRL2(_X_)                            (_X_)

/* bits definitions for register REG_AHB_RF_CP2_AHB_MTX_CTL3 */
#define BITS_MATRIX_CTRL3(_X_)                            (_X_)

/* bits definitions for register REG_AHB_RF_CP2_AHB_MTX_CTL4 */
#define BITS_MATRIX_CTRL4(_X_)                            (_X_)

/* bits definitions for register REG_AHB_RF_CP2_AHB_MTX_CTL5 */
#define BITS_MATRIX_CTRL5(_X_)                            (_X_)

/* bits definitions for register REG_AHB_RF_CP2_AHB_MTX_CTL6 */
#define BITS_MATRIX_CTRL6(_X_)                            (_X_)

/* bits definitions for register REG_AHB_RF_CP2_AHB_ARCH_PORT */
#define BIT_MCU_PROT                                      ( BIT(16) )
#define BITS_ARCH_PROT_VAL(_X_)                           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AHB_RF_CP2_AHB_CHIP_ID */
#define BITS_ID_BASE(_X_)                                 ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_MX_REG(_X_)                                  ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

#endif
