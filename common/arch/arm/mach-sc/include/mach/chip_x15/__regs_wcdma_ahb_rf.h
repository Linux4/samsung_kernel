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


#ifndef __H_REGS_WCDMA_AHB_RF_HEADFILE_H__
#define __H_REGS_WCDMA_AHB_RF_HEADFILE_H__ __FILE__

#define	REGS_WCDMA_AHB_RF

/* registers definitions for WCDMA_AHB_RF */
#define REG_WCDMA_AHB_RF_AHB_EB0                          SCI_ADDR(REGS_WCDMA_AHB_RF_BASE, 0x0000)
#define REG_WCDMA_AHB_RF_AHB_RST0                         SCI_ADDR(REGS_WCDMA_AHB_RF_BASE, 0x0004)
#define REG_WCDMA_AHB_RF_ARCH_EB_REG                      SCI_ADDR(REGS_WCDMA_AHB_RF_BASE, 0x0018)
#define REG_WCDMA_AHB_RF_AHB_REMAP                        SCI_ADDR(REGS_WCDMA_AHB_RF_BASE, 0x001C)
#define REG_WCDMA_AHB_RF_AHB_MISC_CTL                     SCI_ADDR(REGS_WCDMA_AHB_RF_BASE, 0x0020)
#define REG_WCDMA_AHB_RF_AHB_MTX_PRI_SEL0                 SCI_ADDR(REGS_WCDMA_AHB_RF_BASE, 0x0024)
#define REG_WCDMA_AHB_RF_AHB_MTX_PRI_SEL1                 SCI_ADDR(REGS_WCDMA_AHB_RF_BASE, 0x0028)
#define REG_WCDMA_AHB_RF_AHB_SLP_CTL                      SCI_ADDR(REGS_WCDMA_AHB_RF_BASE, 0x0034)
#define REG_WCDMA_AHB_RF_CHIP_ID                          SCI_ADDR(REGS_WCDMA_AHB_RF_BASE, 0x01FC)



/* bits definitions for register REG_WCDMA_AHB_RF_AHB_EB0 */
#define BITS_WCDMA_EN(_X_)                                ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )

/* bits definitions for register REG_WCDMA_AHB_RF_AHB_RST0 */
#define BIT_WCDMA_SOFT_RST                                ( BIT(3) )

/* bits definitions for register REG_WCDMA_AHB_RF_ARCH_EB_REG */
#define BIT_AHB_ARCH_EB                                   ( BIT(0) )

/* bits definitions for register REG_WCDMA_AHB_RF_AHB_REMAP */

/* bits definitions for register REG_WCDMA_AHB_RF_AHB_MISC_CTL */
#define BIT_W_DMA_LSLP_EN                                 ( BIT(2) )

/* bits definitions for register REG_WCDMA_AHB_RF_AHB_MTX_PRI_SEL0 */
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

/* bits definitions for register REG_WCDMA_AHB_RF_AHB_MTX_PRI_SEL1 */
#define BITS_ARMMTX_M6TOS0_RND_THR(_X_)                   ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BIT_ARMMTX_M6TOS0_ADJ_EN                          ( BIT(19) )
#define BIT_ARMMTX_M6TOS0_RND_EN                          ( BIT(18) )
#define BITS_ARMMTX_M6TOS0_SEL(_X_)                       ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_ARMMTX_M5TOS0_RND_THR(_X_)                   ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_ARMMTX_M5TOS0_ADJ_EN                          ( BIT(11) )
#define BIT_ARMMTX_M5TOS0_RND_EN                          ( BIT(10) )
#define BITS_ARMMTX_M5TOS0_SEL(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ARMMTX_M4TOS0_RND_THR(_X_)                   ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_ARMMTX_M4TOS0_ADJ_EN                          ( BIT(3) )
#define BIT_ARMMTX_M4TOS0_RND_EN                          ( BIT(2) )
#define BITS_ARMMTX_M4TOS0_SEL(_X_)                       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_WCDMA_AHB_RF_AHB_SLP_CTL */
#define BIT_MCUMTX_AUTO_GATE_EN                           ( BIT(4) )
#define BIT_AHB_AUTO_GATE_EN                              ( BIT(2) )

/* bits definitions for register REG_WCDMA_AHB_RF_CHIP_ID */
#define BITS_CHIP_ID(_X_)                                 (_X_)

#endif
