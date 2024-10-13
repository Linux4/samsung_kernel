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

#ifndef __REGS_PMU_APB_H__
#define __REGS_PMU_APB_H__

#ifndef __SCI_GLB_REGS_H__
#error  "Don't include this file directly, include <mach/sci_glb_regs.h>"
#endif

#define REGS_PMU_APB

/* registers definitions for controller REGS_PMU_APB */
#define REG_PMU_APB_PD_CA7_TOP_CFG      SCI_ADDR(REGS_PMU_APB_BASE, 0x0000)
#define REG_PMU_APB_PD_CA7_C0_CFG       SCI_ADDR(REGS_PMU_APB_BASE, 0x0004)
#define REG_PMU_APB_PD_CA7_C1_CFG       SCI_ADDR(REGS_PMU_APB_BASE, 0x0008)
#define REG_PMU_APB_PD_CA7_C2_CFG       SCI_ADDR(REGS_PMU_APB_BASE, 0x000C)
#define REG_PMU_APB_PD_CA7_C3_CFG       SCI_ADDR(REGS_PMU_APB_BASE, 0x0010)
#define REG_PMU_APB_PD_AP_DISP_CFG      SCI_ADDR(REGS_PMU_APB_BASE, 0x0014)
#define REG_PMU_APB_PD_AP_SYS_CFG       SCI_ADDR(REGS_PMU_APB_BASE, 0x0018)
#define REG_PMU_APB_PD_MM_TOP_CFG       SCI_ADDR(REGS_PMU_APB_BASE, 0x001C)
#define REG_PMU_APB_PD_GPU_TOP_CFG      SCI_ADDR(REGS_PMU_APB_BASE, 0x0020)
#define REG_PMU_APB_PD_CP0_ARM9_0_CFG   SCI_ADDR(REGS_PMU_APB_BASE, 0x0024)
#define REG_PMU_APB_PD_CP0_ARM9_1_CFG   SCI_ADDR(REGS_PMU_APB_BASE, 0x0028)
#define REG_PMU_APB_PD_CP0_ARM9_2_CFG   SCI_ADDR(REGS_PMU_APB_BASE, 0x002C)
#define REG_PMU_APB_PD_CP0_HU3GE_CFG    SCI_ADDR(REGS_PMU_APB_BASE, 0x0030)
#define REG_PMU_APB_PD_CP0_GSM_CFG      SCI_ADDR(REGS_PMU_APB_BASE, 0x0034)
#define REG_PMU_APB_PD_CP0_L1RAM_CFG    SCI_ADDR(REGS_PMU_APB_BASE, 0x0038)
#define REG_PMU_APB_PD_CP0_SYS_CFG      SCI_ADDR(REGS_PMU_APB_BASE, 0x003C)
#define REG_PMU_APB_PD_CP1_ARM9_CFG     SCI_ADDR(REGS_PMU_APB_BASE, 0x0040)
#define REG_PMU_APB_PD_CP1_GSM_CFG      SCI_ADDR(REGS_PMU_APB_BASE, 0x0044)
#define REG_PMU_APB_PD_CP1_TD_CFG       SCI_ADDR(REGS_PMU_APB_BASE, 0x0048)
#define REG_PMU_APB_PD_CP1_L1RAM_CFG    SCI_ADDR(REGS_PMU_APB_BASE, 0x004C)
#define REG_PMU_APB_PD_CP1_SYS_CFG      SCI_ADDR(REGS_PMU_APB_BASE, 0x0050)
#define REG_PMU_APB_PD_CP2_ARM9_CFG     SCI_ADDR(REGS_PMU_APB_BASE, 0x0054)
#define REG_PMU_APB_PD_CP2_WIFI_CFG     SCI_ADDR(REGS_PMU_APB_BASE, 0x0058)
#define REG_PMU_APB_AP_WAKEUP_POR_CFG   SCI_ADDR(REGS_PMU_APB_BASE, 0x005C)
#define REG_PMU_APB_PD_CP2_SYS_CFG      SCI_ADDR(REGS_PMU_APB_BASE, 0x0060)
#define REG_PMU_APB_PD_PUB_SYS_CFG      SCI_ADDR(REGS_PMU_APB_BASE, 0x0064)
#define REG_PMU_APB_XTL_WAIT_CNT        SCI_ADDR(REGS_PMU_APB_BASE, 0x0068)
#define REG_PMU_APB_XTLBUF_WAIT_CNT     SCI_ADDR(REGS_PMU_APB_BASE, 0x006C)
#define REG_PMU_APB_PLL_WAIT_CNT1       SCI_ADDR(REGS_PMU_APB_BASE, 0x0070)
#define REG_PMU_APB_PLL_WAIT_CNT2       SCI_ADDR(REGS_PMU_APB_BASE, 0x0074)
#define REG_PMU_APB_XTL0_REL_CFG        SCI_ADDR(REGS_PMU_APB_BASE, 0x0078)
#define REG_PMU_APB_XTL1_REL_CFG        SCI_ADDR(REGS_PMU_APB_BASE, 0x007C)
#define REG_PMU_APB_XTL2_REL_CFG        SCI_ADDR(REGS_PMU_APB_BASE, 0x0080)
#define REG_PMU_APB_XTLBUF0_REL_CFG     SCI_ADDR(REGS_PMU_APB_BASE, 0x0084)
#define REG_PMU_APB_XTLBUF1_REL_CFG     SCI_ADDR(REGS_PMU_APB_BASE, 0x0088)
#define REG_PMU_APB_MPLL_REL_CFG        SCI_ADDR(REGS_PMU_APB_BASE, 0x008C)
#define REG_PMU_APB_DPLL_REL_CFG        SCI_ADDR(REGS_PMU_APB_BASE, 0x0090)
#define REG_PMU_APB_TDPLL_REL_CFG       SCI_ADDR(REGS_PMU_APB_BASE, 0x0094)
#define REG_PMU_APB_WPLL_REL_CFG        SCI_ADDR(REGS_PMU_APB_BASE, 0x0098)
#define REG_PMU_APB_CPLL_REL_CFG        SCI_ADDR(REGS_PMU_APB_BASE, 0x009C)
#define REG_PMU_APB_WIFIPLL1_REL_CFG    SCI_ADDR(REGS_PMU_APB_BASE, 0x00A0)
#define REG_PMU_APB_WIFIPLL2_REL_CFG    SCI_ADDR(REGS_PMU_APB_BASE, 0x00A4)
#define REG_PMU_APB_CP_SOFT_RST         SCI_ADDR(REGS_PMU_APB_BASE, 0x00A8)
#define REG_PMU_APB_CP_SLP_STATUS_DBG0  SCI_ADDR(REGS_PMU_APB_BASE, 0x00AC)
#define REG_PMU_APB_CP_SLP_STATUS_DBG1  SCI_ADDR(REGS_PMU_APB_BASE, 0x00B0)
#define REG_PMU_APB_PWR_STATUS0_DBG     SCI_ADDR(REGS_PMU_APB_BASE, 0x00B4)
#define REG_PMU_APB_PWR_STATUS1_DBG     SCI_ADDR(REGS_PMU_APB_BASE, 0x00B8)
#define REG_PMU_APB_PWR_STATUS2_DBG     SCI_ADDR(REGS_PMU_APB_BASE, 0x00BC)
#define REG_PMU_APB_PWR_STATUS3_DBG     SCI_ADDR(REGS_PMU_APB_BASE, 0x00C0)
#define REG_PMU_APB_SLEEP_CTRL          SCI_ADDR(REGS_PMU_APB_BASE, 0x00C4)
#define REG_PMU_APB_DDR_SLEEP_CTRL      SCI_ADDR(REGS_PMU_APB_BASE, 0x00C8)
#define REG_PMU_APB_SLEEP_STATUS        SCI_ADDR(REGS_PMU_APB_BASE, 0x00CC)
#define REG_PMU_APB_PLL_DIV_AUTO_GATE_EN SCI_ADDR(REGS_PMU_APB_BASE, 0x00D0)
#define REG_PMU_APB_PLL_DIV_EN1         SCI_ADDR(REGS_PMU_APB_BASE, 0x00D4)
#define REG_PMU_APB_PLL_DIV_EN2         SCI_ADDR(REGS_PMU_APB_BASE, 0x00D8)
#define REG_PMU_APB_CA7_TOP_CFG         SCI_ADDR(REGS_PMU_APB_BASE, 0x00DC)
#define REG_PMU_APB_CA7_C0_CFG          SCI_ADDR(REGS_PMU_APB_BASE, 0x00E0)
#define REG_PMU_APB_CA7_C1_CFG          SCI_ADDR(REGS_PMU_APB_BASE, 0x00E4)
#define REG_PMU_APB_CA7_C2_CFG          SCI_ADDR(REGS_PMU_APB_BASE, 0x00E8)
#define REG_PMU_APB_CA7_C3_CFG          SCI_ADDR(REGS_PMU_APB_BASE, 0x00EC)
#define REG_PMU_APB_DDR_CHN_SLEEP_CTRL0 SCI_ADDR(REGS_PMU_APB_BASE, 0x00F0)
#define REG_PMU_APB_DDR_CHN_SLEEP_CTRL1 SCI_ADDR(REGS_PMU_APB_BASE, 0x00F4)
#define REG_PMU_APB_BISR_CFG            SCI_ADDR(REGS_PMU_APB_BASE, 0x00F8)
#define REG_PMU_APB_CGM_AP_AUTO_GATE_EN SCI_ADDR(REGS_PMU_APB_BASE, 0x00FC)
#define REG_PMU_APB_CGM_GPU_MM_AUTO_GATE_EN SCI_ADDR(REGS_PMU_APB_BASE, 0x0100)
#define REG_PMU_APB_CGM_CP0_AUTO_GATE_EN SCI_ADDR(REGS_PMU_APB_BASE, 0x0104)
#define REG_PMU_APB_CGM_CP1_AUTO_GATE_EN SCI_ADDR(REGS_PMU_APB_BASE, 0x0108)
#define REG_PMU_APB_CGM_CP2_AUTO_GATE_EN SCI_ADDR(REGS_PMU_APB_BASE, 0x010C)
#define REG_PMU_APB_CGM_AP_EN           SCI_ADDR(REGS_PMU_APB_BASE, 0x00110)
#define REG_PMU_APB_CGM_GPU_MM_EN       SCI_ADDR(REGS_PMU_APB_BASE, 0x0114)
#define REG_PMU_APB_CGM_CP0_EN          SCI_ADDR(REGS_PMU_APB_BASE, 0x0118)
#define REG_PMU_APB_CGM_CP1_EN          SCI_ADDR(REGS_PMU_APB_BASE, 0x011C)
#define REG_PMU_APB_CGM_CP2_EN          SCI_ADDR(REGS_PMU_APB_BASE, 0x0120)
#define REG_PMU_APB_DDR_OP_MODE_CFG     SCI_ADDR(REGS_PMU_APB_BASE, 0x0124)
#define REG_PMU_APB_DDR_PHY_RET_CFG     SCI_ADDR(REGS_PMU_APB_BASE, 0x0128)

/* bits definitions for register REG_PMU_APB_PD_CA7_TOP_CFG */
#define BIT_PD_CA7_TOP_DBG_SHUTDOWN_EN  ( BIT(28) )
#define BIT_PD_CA7_TOP_FORCE_SHUTDOWN   ( BIT(25) )
#define BIT_PD_CA7_TOP_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CA7_TOP_PWR_ON_DLY(_x_) ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CA7_TOP_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CA7_TOP_ISO_ON_DLY(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CA7_C0_CFG */
#define BIT_PD_CA7_C0_DBG_SHUTDOWN_EN   ( BIT(28) )
#define BIT_PD_CA7_C0_FORCE_SHUTDOWN    ( BIT(25) )
#define BIT_PD_CA7_C0_AUTO_SHUTDOWN_EN  ( BIT(24) )
#define BITS_PD_CA7_C0_PWR_ON_DLY(_x_)  ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CA7_C0_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CA7_C0_ISO_ON_DLY(_x_)  ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CA7_C1_CFG */
#define BIT_PD_CA7_C1_DBG_SHUTDOWN_EN   ( BIT(28) )
#define BIT_PD_CA7_C1_FORCE_SHUTDOWN    ( BIT(25) )
#define BIT_PD_CA7_C1_AUTO_SHUTDOWN_EN  ( BIT(24) )
#define BITS_PD_CA7_C1_PWR_ON_DLY(_x_)  ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CA7_C1_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CA7_C1_ISO_ON_DLY(_x_)  ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CA7_C2_CFG */
#define BIT_PD_CA7_C2_DBG_SHUTDOWN_EN   ( BIT(28) )
#define BIT_PD_CA7_C2_FORCE_SHUTDOWN    ( BIT(25) )
#define BIT_PD_CA7_C2_AUTO_SHUTDOWN_EN  ( BIT(24) )
#define BITS_PD_CA7_C2_PWR_ON_DLY(_x_)  ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CA7_C2_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CA7_C2_ISO_ON_DLY(_x_)  ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CA7_C3_CFG */
#define BIT_PD_CA7_C3_DBG_SHUTDOWN_EN   ( BIT(28) )
#define BIT_PD_CA7_C3_FORCE_SHUTDOWN    ( BIT(25) )
#define BIT_PD_CA7_C3_AUTO_SHUTDOWN_EN  ( BIT(24) )
#define BITS_PD_CA7_C3_PWR_ON_DLY(_x_)  ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CA7_C3_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CA7_C3_ISO_ON_DLY(_x_)  ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_AP_DISP_CFG */

/* bits definitions for register REG_PMU_APB_PD_AP_SYS_CFG */
#define BIT_PD_AP_SYS_FORCE_SHUTDOWN    ( BIT(25) )
#define BIT_PD_AP_SYS_AUTO_SHUTDOWN_EN  ( BIT(24) )
#define BITS_PD_AP_SYS_PWR_ON_DLY(_x_)  ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_AP_SYS_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_AP_SYS_ISO_ON_DLY(_x_)  ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_MM_TOP_CFG */
#define BIT_PD_MM_TOP_FORCE_SHUTDOWN    ( BIT(25) )
#define BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN  ( BIT(24) )
#define BITS_PD_MM_TOP_PWR_ON_DLY(_x_)  ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_MM_TOP_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_MM_TOP_ISO_ON_DLY(_x_)  ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_GPU_TOP_CFG */
#define BIT_PD_GPU_TOP_FORCE_SHUTDOWN   ( BIT(25) )
#define BIT_PD_GPU_TOP_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_GPU_TOP_PWR_ON_DLY(_x_) ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_GPU_TOP_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_GPU_TOP_ISO_ON_DLY(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP0_ARM9_0_CFG */
#define BIT_PD_CP0_ARM9_0_FORCE_SHUTDOWN ( BIT(25) )
#define BIT_PD_CP0_ARM9_0_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP0_ARM9_0_PWR_ON_DLY(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP0_ARM9_0_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP0_ARM9_0_ISO_ON_DLY(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP0_ARM9_1_CFG */
#define BIT_PD_CP0_ARM9_1_FORCE_SHUTDOWN ( BIT(25) )
#define BIT_PD_CP0_ARM9_1_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP0_ARM9_1_PWR_ON_DLY(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP0_ARM9_1_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP0_ARM9_1_ISO_ON_DLY(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP0_ARM9_2_CFG */
#define BIT_PD_CP0_ARM9_2_FORCE_SHUTDOWN ( BIT(25) )
#define BIT_PD_CP0_ARM9_2_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP0_ARM9_2_PWR_ON_DLY(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP0_ARM9_2_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP0_ARM9_2_ISO_ON_DLY(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP0_HU3GE_CFG */
#define BIT_PD_CP0_HU3GE_FORCE_SHUTDOWN ( BIT(25) )
#define BIT_PD_CP0_HU3GE_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP0_HU3GE_PWR_ON_DLY(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP0_HU3GE_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP0_HU3GE_ISO_ON_DLY(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP0_GSM_CFG */
#define BIT_PD_CP0_GSM_FORCE_SHUTDOWN   ( BIT(25) )
#define BIT_PD_CP0_GSM_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP0_GSM_PWR_ON_DLY(_x_) ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP0_GSM_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP0_GSM_ISO_ON_DLY(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP0_L1RAM_CFG */
#define BIT_PD_CP0_L1RAM_FORCE_SHUTDOWN ( BIT(25) )
#define BIT_PD_CP0_L1RAM_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP0_L1RAM_PWR_ON_DLY(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP0_L1RAM_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP0_L1RAM_ISO_ON_DLY(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP0_SYS_CFG */
#define BIT_CP0_FORCE_DEEP_SLEEP        ( BIT(28) )
#define BIT_PD_CP0_SYS_FORCE_SHUTDOWN   ( BIT(25) )
#define BITS_PD_CP0_SYS_PWR_ON_DLY(_x_) ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP0_SYS_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP0_SYS_ISO_ON_DLY(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP1_ARM9_CFG */
#define BIT_PD_CP1_ARM9_FORCE_SHUTDOWN  ( BIT(25) )
#define BIT_PD_CP1_ARM9_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP1_ARM9_PWR_ON_DLY(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP1_ARM9_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP1_ARM9_ISO_ON_DLY(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP1_GSM_CFG */
#define BIT_PD_CP1_GSM_FORCE_SHUTDOWN   ( BIT(25) )
#define BIT_PD_CP1_GSM_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP1_GSM_PWR_ON_DLY(_x_) ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP1_GSM_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP1_GSM_ISO_ON_DLY(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP1_TD_CFG */
#define BIT_PD_CP1_TD_FORCE_SHUTDOWN    ( BIT(25) )
#define BIT_PD_CP1_TD_AUTO_SHUTDOWN_EN  ( BIT(24) )
#define BITS_PD_CP1_TD_PWR_ON_DLY(_x_)  ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP1_TD_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP1_TD_ISO_ON_DLY(_x_)  ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP1_L1RAM_CFG */
#define BIT_PD_CP1_L1RAM_FORCE_SHUTDOWN ( BIT(25) )
#define BIT_PD_CP1_L1RAM_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP1_L1RAM_PWR_ON_DLY(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP1_L1RAM_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP1_L1RAM_ISO_ON_DLY(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP1_SYS_CFG */
#define BIT_CP1_FORCE_DEEP_SLEEP        ( BIT(28) )
#define BIT_PD_CP1_SYS_FORCE_SHUTDOWN   ( BIT(25) )
#define BITS_PD_CP1_SYS_PWR_ON_DLY(_x_) ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP1_SYS_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP1_SYS_ISO_ON_DLY(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP2_ARM9_CFG */
#define BIT_PD_CP2_ARM9_FORCE_SHUTDOWN  ( BIT(25) )
#define BIT_PD_CP2_ARM9_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP2_ARM9_PWR_ON_DLY(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP2_ARM9_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP2_ARM9_ISO_ON_DLY(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_CP2_WIFI_CFG */
#define BIT_PD_CP2_WIFI_FORCE_SHUTDOWN  ( BIT(25) )
#define BIT_PD_CP2_WIFI_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_CP2_WIFI_PWR_ON_DLY(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP2_WIFI_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP2_WIFI_ISO_ON_DLY(_x_)( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_AP_WAKEUP_POR_CFG */
#define BIT_AP_WAKEUP_POR_N             ( BIT(0) )

/* bits definitions for register REG_PMU_APB_PD_CP2_SYS_CFG */
#define BIT_CP2_FORCE_DEEP_SLEEP        ( BIT(28) )
#define BIT_PD_CP2_SYS_FORCE_SHUTDOWN   ( BIT(25) )
#define BITS_PD_CP2_SYS_PWR_ON_DLY(_x_) ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP2_SYS_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP2_SYS_ISO_ON_DLY(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PD_PUB_SYS_CFG */
#define BIT_PD_PUB_SYS_FORCE_SHUTDOWN   ( BIT(25) )
#define BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN ( BIT(24) )
#define BITS_PD_PUB_SYS_PWR_ON_DLY(_x_) ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_PUB_SYS_PWR_ON_SEQ_DLY(_x_)( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_PUB_SYS_ISO_ON_DLY(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_XTL_WAIT_CNT */
#define BITS_XTL1_WAIT_CNT(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_XTL0_WAIT_CNT(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_XTLBUF_WAIT_CNT */
#define BITS_XTLBUF1_WAIT_CNT(_x_)      ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_XTLBUF0_WAIT_CNT(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PLL_WAIT_CNT1 */
#define BITS_WPLL_WAIT_CNT(_x_)         ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_TDPLL_WAIT_CNT(_x_)        ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_DPLL_WAIT_CNT(_x_)         ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_MPLL_WAIT_CNT(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_PLL_WAIT_CNT2 */
#define BITS_WIFIPLL2_WAIT_CNT(_x_)     ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_WIFIPLL1_WAIT_CNT(_x_)     ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CPLL_WAIT_CNT(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_PMU_APB_XTL0_REL_CFG */
#define BIT_XTL0_CP2_SEL                ( BIT(3) )
#define BIT_XTL0_CP1_SEL                ( BIT(2) )
#define BIT_XTL0_CP0_SEL                ( BIT(1) )
#define BIT_XTL0_AP_SEL                 ( BIT(0) )

/* bits definitions for register REG_PMU_APB_XTL1_REL_CFG */
#define BIT_XTL1_CP2_SEL                ( BIT(3) )
#define BIT_XTL1_CP1_SEL                ( BIT(2) )
#define BIT_XTL1_CP0_SEL                ( BIT(1) )
#define BIT_XTL1_AP_SEL                 ( BIT(0) )

/* bits definitions for register REG_PMU_APB_XTL2_REL_CFG */
#define BIT_XTL2_CP2_SEL                ( BIT(3) )
#define BIT_XTL2_CP1_SEL                ( BIT(2) )
#define BIT_XTL2_CP0_SEL                ( BIT(1) )
#define BIT_XTL2_AP_SEL                 ( BIT(0) )

/* bits definitions for register REG_PMU_APB_XTLBUF0_REL_CFG */
#define BIT_XTLBUF0_CP2_SEL             ( BIT(3) )
#define BIT_XTLBUF0_CP1_SEL             ( BIT(2) )
#define BIT_XTLBUF0_CP0_SEL             ( BIT(1) )
#define BIT_XTLBUF0_AP_SEL              ( BIT(0) )

/* bits definitions for register REG_PMU_APB_XTLBUF1_REL_CFG */
#define BIT_XTLBUF1_CP2_SEL             ( BIT(3) )
#define BIT_XTLBUF1_CP1_SEL             ( BIT(2) )
#define BIT_XTLBUF1_CP0_SEL             ( BIT(1) )
#define BIT_XTLBUF1_AP_SEL              ( BIT(0) )

/* bits definitions for register REG_PMU_APB_MPLL_REL_CFG */
#define BIT_MPLL_REF_SEL                ( BIT(4) )
#define BIT_MPLL_CP2_SEL                ( BIT(3) )
#define BIT_MPLL_CP1_SEL                ( BIT(2) )
#define BIT_MPLL_CP0_SEL                ( BIT(1) )
#define BIT_MPLL_AP_SEL                 ( BIT(0) )

/* bits definitions for register REG_PMU_APB_DPLL_REL_CFG */
#define BIT_DPLL_REF_SEL                ( BIT(4) )
#define BIT_DPLL_CP2_SEL                ( BIT(3) )
#define BIT_DPLL_CP1_SEL                ( BIT(2) )
#define BIT_DPLL_CP0_SEL                ( BIT(1) )
#define BIT_DPLL_AP_SEL                 ( BIT(0) )

/* bits definitions for register REG_PMU_APB_TDPLL_REL_CFG */
#define BIT_TDPLL_REF_SEL               ( BIT(4) )
#define BIT_TDPLL_CP2_SEL               ( BIT(3) )
#define BIT_TDPLL_CP1_SEL               ( BIT(2) )
#define BIT_TDPLL_CP0_SEL               ( BIT(1) )
#define BIT_TDPLL_AP_SEL                ( BIT(0) )

/* bits definitions for register REG_PMU_APB_WPLL_REL_CFG */
#define BIT_WPLL_REF_SEL                ( BIT(4) )
#define BIT_WPLL_CP2_SEL                ( BIT(3) )
#define BIT_WPLL_CP1_SEL                ( BIT(2) )
#define BIT_WPLL_CP0_SEL                ( BIT(1) )
#define BIT_WPLL_AP_SEL                 ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CPLL_REL_CFG */
#define BIT_CPLL_REF_SEL                ( BIT(4) )
#define BIT_CPLL_CP2_SEL                ( BIT(3) )
#define BIT_CPLL_CP1_SEL                ( BIT(2) )
#define BIT_CPLL_CP0_SEL                ( BIT(1) )
#define BIT_CPLL_AP_SEL                 ( BIT(0) )

/* bits definitions for register REG_PMU_APB_WIFIPLL1_REL_CFG */
#define BIT_WIFIPLL1_REF_SEL            ( BIT(4) )
#define BIT_WIFIPLL1_CP2_SEL            ( BIT(3) )
#define BIT_WIFIPLL1_CP1_SEL            ( BIT(2) )
#define BIT_WIFIPLL1_CP0_SEL            ( BIT(1) )
#define BIT_WIFIPLL1_AP_SEL             ( BIT(0) )

/* bits definitions for register REG_PMU_APB_WIFIPLL2_REL_CFG */
#define BIT_WIFIPLL2_REF_SEL            ( BIT(4) )
#define BIT_WIFIPLL2_CP2_SEL            ( BIT(3) )
#define BIT_WIFIPLL2_CP1_SEL            ( BIT(2) )
#define BIT_WIFIPLL2_CP0_SEL            ( BIT(1) )
#define BIT_WIFIPLL2_AP_SEL             ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CP_SOFT_RST */
#define BIT_PUB_SOFT_RST                ( BIT(6) )
#define BIT_AP_SOFT_RST                 ( BIT(5) )
#define BIT_GPU_SOFT_RST                ( BIT(4) )
#define BIT_MM_SOFT_RST                 ( BIT(3) )
#define BIT_CP2_SOFT_RST                ( BIT(2) )
#define BIT_CP1_SOFT_RST                ( BIT(1) )
#define BIT_CP0_SOFT_RST                ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CP_SLP_STATUS_DBG0 */
#define BITS_CP1_DEEP_SLP_DBG(_x_)      ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_CP0_DEEP_SLP_DBG(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_PMU_APB_CP_SLP_STATUS_DBG1 */
#define BITS_CP2_DEEP_SLP_DBG(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_PMU_APB_PWR_STATUS0_DBG */
#define BITS_PD_MM_TOP_STATE(_x_)       ( (_x_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PD_GPU_TOP_STATE(_x_)      ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_PD_CA7_C3_STATE(_x_)       ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_PD_CA7_C2_STATE(_x_)       ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CA7_C1_STATE(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_PD_CA7_C0_STATE(_x_)       ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PD_CA7_TOP_STATE(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_PMU_APB_PWR_STATUS1_DBG */
#define BITS_PD_CP0_SYS_STATE(_x_)      ( (_x_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_PD_CP0_L1RAM_STATE(_x_)    ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_PD_CP0_GSM_STATE(_x_)      ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP0_HU3GE_STATE(_x_)    ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_PD_CP0_ARM9_2_STATE(_x_)   ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP0_ARM9_1_STATE(_x_)   ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_PD_CP0_ARM9_0_STATE(_x_)   ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PD_AP_SYS_STATE(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_PMU_APB_PWR_STATUS2_DBG */
#define BITS_PD_CP2_WIFI_STATE(_x_)     ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)) )
#define BITS_PD_CP2_ARM9_STATE(_x_)     ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_PD_CP1_SYS_STATE(_x_)      ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_PD_CP1_L1RAM_STATE(_x_)    ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_PD_CP1_TD_STATE(_x_)       ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_PD_CP1_GSM_STATE(_x_)      ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PD_CP1_ARM9_STATE(_x_)     ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_PMU_APB_PWR_STATUS3_DBG */
#define BITS_PD_PUB_SYS_STATE(_x_)      ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_PD_CP2_SYS_STATE(_x_)      ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_PMU_APB_SLEEP_CTRL */
#define BIT_CP2_SLEEP_XTL_ON            ( BIT(11) )
#define BIT_CP1_SLEEP_XTL_ON            ( BIT(10) )
#define BIT_CP0_SLEEP_XTL_ON            ( BIT(9) )
#define BIT_AP_SLEEP_XTL_ON             ( BIT(8) )
#define BIT_DISP_DEEP_SLEEP             ( BIT(6) )
#define BIT_GPU_DEEP_SLEEP              ( BIT(5) )
#define BIT_MM_DEEP_SLEEP               ( BIT(4) )
#define BIT_CP2_DEEP_SLEEP              ( BIT(3) )
#define BIT_CP1_DEEP_SLEEP              ( BIT(2) )
#define BIT_CP0_DEEP_SLEEP              ( BIT(1) )
#define BIT_AP_DEEP_SLEEP               ( BIT(0) )

/* bits definitions for register REG_PMU_APB_DDR_SLEEP_CTRL */
#define BIT_DDR_PUBL_APB_SOFT_RST       ( BIT(12) )
#define BIT_DDR_UMCTL_APB_SOFT_RST      ( BIT(11) )
#define BIT_DDR_PUBL_SOFT_RST           ( BIT(10) )
#define BIT_DDR_UMCTL_SOFT_RST          ( BIT(9) )
#define BIT_DDR_PHY_SOFT_RST            ( BIT(8) )
#define BIT_DDR_PHY_AUTO_GATE_EN        ( BIT(6) )
#define BIT_DDR_PUBL_AUTO_GATE_EN       ( BIT(5) )
#define BIT_DDR_UMCTL_AUTO_GATE_EN      ( BIT(4) )
#define BIT_DDR_PHY_EB                  ( BIT(2) )
#define BIT_DDR_UMCTL_EB                ( BIT(1) )
#define BIT_DDR_PUBL_EB                 ( BIT(0) )

/* bits definitions for register REG_PMU_APB_SLEEP_STATUS */
#define BITS_CP2_SLP_STATUS(_x_)        ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_CP1_SLP_STATUS(_x_)        ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CP0_SLP_STATUS(_x_)        ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_AP_SLP_STATUS(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_PMU_APB_PLL_DIV_AUTO_GATE_EN */
#define BIT_WIFIPLL2_DIV_AUTO_GATE_EN   ( BIT(6) )
#define BIT_WIFIPLL1_DIV_AUTO_GATE_EN   ( BIT(5) )
#define BIT_WPLL_DIV_AUTO_GATE_EN       ( BIT(4) )
#define BIT_TDPLL_DIV_AUTO_GATE_EN      ( BIT(3) )
#define BIT_CPLL_DIV_AUTO_GATE_EN       ( BIT(2) )
#define BIT_DPLL_DIV_AUTO_GATE_EN       ( BIT(1) )
#define BIT_MPLL_DIV_AUTO_GATE_EN       ( BIT(0) )

/* bits definitions for register REG_PMU_APB_PLL_DIV_EN1 */
#define BIT_WIFIPLL2_80M_EN             ( BIT(31) )
#define BIT_WIFIPLL2_160M_EN            ( BIT(30) )
#define BIT_WIFIPLL2_120M_EN            ( BIT(29) )
#define BIT_WIFIPLL1_20M_EN             ( BIT(28) )
#define BIT_WIFIPLL1_40M_EN             ( BIT(27) )
#define BIT_WIFIPLL1_80M_EN             ( BIT(26) )
#define BIT_WIFIPLL1_44M_EN             ( BIT(25) )
#define BIT_WPLL_76M8_EN                ( BIT(24) )
#define BIT_WPLL_51M2_EN                ( BIT(23) )
#define BIT_WPLL_102M4_EN               ( BIT(22) )
#define BIT_WPLL_307M2_EN               ( BIT(21) )
#define BIT_WPLL_460M8_EN               ( BIT(20) )
#define BIT_CPLL_52M_EN                 ( BIT(19) )
#define BIT_CPLL_104M_EN                ( BIT(18) )
#define BIT_CPLL_208M_EN                ( BIT(17) )
#define BIT_CPLL_312M_EN                ( BIT(16) )
#define BIT_TDPLL_38M4_EN               ( BIT(15) )
#define BIT_TDPLL_76M8_EN               ( BIT(14) )
#define BIT_TDPLL_51M2_EN               ( BIT(13) )
#define BIT_TDPLL_153M6_EN              ( BIT(12) )
#define BIT_TDPLL_64M_EN                ( BIT(11) )
#define BIT_TDPLL_128M_EN               ( BIT(10) )
#define BIT_TDPLL_256M_EN               ( BIT(9) )
#define BIT_TDPLL_12M_EN                ( BIT(8) )
#define BIT_TDPLL_24M_EN                ( BIT(7) )
#define BIT_TDPLL_48M_EN                ( BIT(6) )
#define BIT_TDPLL_96M_EN                ( BIT(5) )
#define BIT_TDPLL_192M_EN               ( BIT(4) )
#define BIT_TDPLL_384M_EN               ( BIT(3) )
#define BIT_DPLL_44M_EN                 ( BIT(2) )
#define BIT_MPLL_37M5_EN                ( BIT(1) )
#define BIT_MPLL_300M_EN                ( BIT(0) )

/* bits definitions for register REG_PMU_APB_PLL_DIV_EN2 */
#define BIT_WIFIPLL2_20M_EN             ( BIT(1) )
#define BIT_WIFIPLL2_40M_EN             ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CA7_TOP_CFG */
#define BIT_CA7_L2RSTDISABLE            ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CA7_C0_CFG */
#define BIT_CA7_VINITHI_C0              ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CA7_C1_CFG */
#define BIT_CA7_VINITHI_C1              ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CA7_C2_CFG */
#define BIT_CA7_VINITHI_C2              ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CA7_C3_CFG */
#define BIT_CA7_VINITHI_C3              ( BIT(0) )

/* bits definitions for register REG_PMU_APB_DDR_CHN_SLEEP_CTRL0 */
#define BIT_DDR_CHN9_AXI_LP_EN          ( BIT(25) )
#define BIT_DDR_CHN8_AXI_LP_EN          ( BIT(24) )
#define BIT_DDR_CHN7_AXI_LP_EN          ( BIT(23) )
#define BIT_DDR_CHN6_AXI_LP_EN          ( BIT(22) )
#define BIT_DDR_CHN5_AXI_LP_EN          ( BIT(21) )
#define BIT_DDR_CHN4_AXI_LP_EN          ( BIT(20) )
#define BIT_DDR_CHN3_AXI_LP_EN          ( BIT(19) )
#define BIT_DDR_CHN2_AXI_LP_EN          ( BIT(18) )
#define BIT_DDR_CHN1_AXI_LP_EN          ( BIT(17) )
#define BIT_DDR_CHN0_AXI_LP_EN          ( BIT(16) )
#define BIT_DDR_CHN9_CGM_SEL            ( BIT(9) )
#define BIT_DDR_CHN8_CGM_SEL            ( BIT(8) )
#define BIT_DDR_CHN7_CGM_SEL            ( BIT(7) )
#define BIT_DDR_CHN6_CGM_SEL            ( BIT(6) )
#define BIT_DDR_CHN5_CGM_SEL            ( BIT(5) )
#define BIT_DDR_CHN4_CGM_SEL            ( BIT(4) )
#define BIT_DDR_CHN3_CGM_SEL            ( BIT(3) )
#define BIT_DDR_CHN2_CGM_SEL            ( BIT(2) )
#define BIT_DDR_CHN1_CGM_SEL            ( BIT(1) )
#define BIT_DDR_CHN0_CGM_SEL            ( BIT(0) )

/* bits definitions for register REG_PMU_APB_DDR_CHN_SLEEP_CTRL1 */
#define BIT_DDR_CHN9_AXI_STOP_SEL       ( BIT(9) )
#define BIT_DDR_CHN8_AXI_STOP_SEL       ( BIT(8) )
#define BIT_DDR_CHN7_AXI_STOP_SEL       ( BIT(7) )
#define BIT_DDR_CHN6_AXI_STOP_SEL       ( BIT(6) )
#define BIT_DDR_CHN5_AXI_STOP_SEL       ( BIT(5) )
#define BIT_DDR_CHN4_AXI_STOP_SEL       ( BIT(4) )
#define BIT_DDR_CHN3_AXI_STOP_SEL       ( BIT(3) )
#define BIT_DDR_CHN2_AXI_STOP_SEL       ( BIT(2) )
#define BIT_DDR_CHN1_AXI_STOP_SEL       ( BIT(1) )
#define BIT_DDR_CHN0_AXI_STOP_SEL       ( BIT(0) )

/* bits definitions for register REG_PMU_APB_BISR_CFG */
#define BIT_PD_CP1_TD_BISR_DONE         ( BIT(29) )
#define BIT_PD_CP1_SYS_BISR_DONE        ( BIT(28) )
#define BIT_PD_CP0_HU3GE_BISR_DONE      ( BIT(27) )
#define BIT_PD_CP0_SYS_BISR_DONE        ( BIT(26) )
#define BIT_PD_MM_TOP_BISR_DONE         ( BIT(25) )
#define BIT_PD_GPU_TOP_BISR_DONE        ( BIT(24) )
#define BIT_PD_CP1_TD_BISR_BUSY         ( BIT(21) )
#define BIT_PD_CP1_SYS_BISR_BUSY        ( BIT(20) )
#define BIT_PD_CP0_HU3GE_BISR_BUSY      ( BIT(19) )
#define BIT_PD_CP0_SYS_BISR_BUSY        ( BIT(18) )
#define BIT_PD_MM_TOP_BISR_BUSY         ( BIT(17) )
#define BIT_PD_GPU_TOP_BISR_BUSY        ( BIT(16) )
#define BIT_PD_CP1_TD_BISR_FORCE_EN     ( BIT(13) )
#define BIT_PD_CP1_SYS_BISR_FORCE_EN    ( BIT(12) )
#define BIT_PD_CP0_HU3GE_BISR_FORCE_EN  ( BIT(11) )
#define BIT_PD_CP0_SYS_BISR_FORCE_EN    ( BIT(10) )
#define BIT_PD_MM_TOP_BISR_FORCE_EN     ( BIT(9) )
#define BIT_PD_GPU_TOP_BISR_FORCE_EN    ( BIT(8) )
#define BIT_PD_CP1_TD_BISR_FORCE_BYP    ( BIT(5) )
#define BIT_PD_CP1_SYS_BISR_FORCE_BYP   ( BIT(4) )
#define BIT_PD_CP0_HU3GE_BISR_FORCE_BYP ( BIT(3) )
#define BIT_PD_CP0_SYS_BISR_FORCE_BYP   ( BIT(2) )
#define BIT_PD_MM_TOP_BISR_FORCE_BYP    ( BIT(1) )
#define BIT_PD_GPU_TOP_BISR_FORCE_BYP   ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_AP_AUTO_GATE_EN */
#define BIT_CGM_12M_AP_AUTO_GATE_EN     ( BIT(19) )
#define BIT_CGM_24M_AP_AUTO_GATE_EN     ( BIT(18) )
#define BIT_CGM_48M_AP_AUTO_GATE_EN     ( BIT(17) )
#define BIT_CGM_51M2_AP_AUTO_GATE_EN    ( BIT(16) )
#define BIT_CGM_64M_AP_AUTO_GATE_EN     ( BIT(15) )
#define BIT_CGM_76M8_AP_AUTO_GATE_EN    ( BIT(14) )
#define BIT_CGM_96M_AP_AUTO_GATE_EN     ( BIT(13) )
#define BIT_CGM_128M_AP_AUTO_GATE_EN    ( BIT(12) )
#define BIT_CGM_153M6_AP_AUTO_GATE_EN   ( BIT(11) )
#define BIT_CGM_192M_AP_AUTO_GATE_EN    ( BIT(10) )
#define BIT_CGM_256M_AP_AUTO_GATE_EN    ( BIT(9) )
#define BIT_CGM_384M_AP_AUTO_GATE_EN    ( BIT(8) )
#define BIT_CGM_312M_AP_AUTO_GATE_EN    ( BIT(7) )
#define BIT_CGM_MPLL_AP_AUTO_GATE_EN    ( BIT(6) )
#define BIT_CGM_WPLL_AP_AUTO_GATE_EN    ( BIT(5) )
#define BIT_CGM_WIFIPLL1_AP_AUTO_GATE_EN ( BIT(4) )
#define BIT_CGM_TDPLL_AP_AUTO_GATE_EN   ( BIT(3) )
#define BIT_CGM_CPLL_AP_AUTO_GATE_EN    ( BIT(2) )
#define BIT_CGM_DPLL_AP_AUTO_GATE_EN    ( BIT(1) )
#define BIT_CGM_26M_AP_AUTO_GATE_EN     ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_GPU_MM_AUTO_GATE_EN */
#define BIT_CGM_12M_MM_AUTO_GATE_EN     ( BIT(26) )
#define BIT_CGM_24M_MM_AUTO_GATE_EN     ( BIT(25) )
#define BIT_CGM_48M_MM_AUTO_GATE_EN     ( BIT(24) )
#define BIT_CGM_64M_MM_AUTO_GATE_EN     ( BIT(23) )
#define BIT_CGM_76M8_MM_AUTO_GATE_EN    ( BIT(22) )
#define BIT_CGM_96M_MM_AUTO_GATE_EN     ( BIT(21) )
#define BIT_CGM_128M_MM_AUTO_GATE_EN    ( BIT(20) )
#define BIT_CGM_153M6_MM_AUTO_GATE_EN   ( BIT(19) )
#define BIT_CGM_192M_MM_AUTO_GATE_EN    ( BIT(18) )
#define BIT_CGM_256M_MM_AUTO_GATE_EN    ( BIT(17) )
#define BIT_CGM_26M_MM_AUTO_GATE_EN     ( BIT(16) )
#define BIT_CGM_256M_GPU_AUTO_GATE_EN   ( BIT(3) )
#define BIT_CGM_208M_GPU_AUTO_GATE_EN   ( BIT(2) )
#define BIT_CGM_312M_GPU_AUTO_GATE_EN   ( BIT(1) )
#define BIT_CGM_300M_GPU_AUTO_GATE_EN   ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_CP0_AUTO_GATE_EN */
#define BIT_CGM_460M8_CP0W_AUTO_GATE_EN ( BIT(13) )
#define BIT_CGM_307M2_CP0W_AUTO_GATE_EN ( BIT(12) )
#define BIT_CGM_51M2_CP0W_AUTO_GATE_EN  ( BIT(11) )
#define BIT_CGM_76M8_CP0W_AUTO_GATE_EN  ( BIT(10) )
#define BIT_CGM_102M4_CP0W_AUTO_GATE_EN ( BIT(9) )
#define BIT_CGM_192M_CP0_AUTO_GATE_EN   ( BIT(8) )
#define BIT_CGM_51M2_CP0_AUTO_GATE_EN   ( BIT(7) )
#define BIT_CGM_76M8_CP0_AUTO_GATE_EN   ( BIT(6) )
#define BIT_CGM_153M6_CP0_AUTO_GATE_EN  ( BIT(5) )
#define BIT_CGM_48M_CP0_AUTO_GATE_EN    ( BIT(4) )
#define BIT_CGM_64M_CP0_AUTO_GATE_EN    ( BIT(3) )
#define BIT_CGM_96M_CP0_AUTO_GATE_EN    ( BIT(2) )
#define BIT_CGM_128M_CP0_AUTO_GATE_EN   ( BIT(1) )
#define BIT_CGM_26M_CP0_AUTO_GATE_EN    ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_CP1_AUTO_GATE_EN */
#define BIT_CGM_312M_CP1_AUTO_GATE_EN   ( BIT(10) )
#define BIT_CGM_256M_CP1_AUTO_GATE_EN   ( BIT(9) )
#define BIT_CGM_192M_CP1_AUTO_GATE_EN   ( BIT(8) )
#define BIT_CGM_51M2_CP1_AUTO_GATE_EN   ( BIT(7) )
#define BIT_CGM_76M8_CP1_AUTO_GATE_EN   ( BIT(6) )
#define BIT_CGM_153M6_CP1_AUTO_GATE_EN  ( BIT(5) )
#define BIT_CGM_48M_CP1_AUTO_GATE_EN    ( BIT(4) )
#define BIT_CGM_96M_CP1_AUTO_GATE_EN    ( BIT(3) )
#define BIT_CGM_64M_CP1_AUTO_GATE_EN    ( BIT(2) )
#define BIT_CGM_128M_CP1_AUTO_GATE_EN   ( BIT(1) )
#define BIT_CGM_26M_CP1_AUTO_GATE_EN    ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_CP2_AUTO_GATE_EN */
#define BIT_CGM_20M_CP2WF2_AUTO_GATE_EN ( BIT(11) )
#define BIT_CGM_80M_CP2WF2_AUTO_GATE_EN ( BIT(10) )
#define BIT_CGM_120M_CP2WF2_AUTO_GATE_EN ( BIT(9) )
#define BIT_CGM_160M_CP2WF2_AUTO_GATE_EN ( BIT(8) )
#define BIT_CGM_20M_CP2WF1_AUTO_GATE_EN ( BIT(7) )
#define BIT_CGM_44M_CP2WF1_AUTO_GATE_EN ( BIT(6) )
#define BIT_CGM_80M_CP2WF1_AUTO_GATE_EN ( BIT(5) )
#define BIT_CGM_256M_CP2_AUTO_GATE_EN   ( BIT(4) )
#define BIT_CGM_104M_CP2_AUTO_GATE_EN   ( BIT(3) )
#define BIT_CGM_208M_CP2_AUTO_GATE_EN   ( BIT(2) )
#define BIT_CGM_312M_CP2_AUTO_GATE_EN   ( BIT(1) )
#define BIT_CGM_26M_CP2_AUTO_GATE_EN    ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_AP_EN */
#define BIT_CGM_12M_AP_EN               ( BIT(19) )
#define BIT_CGM_24M_AP_EN               ( BIT(18) )
#define BIT_CGM_48M_AP_EN               ( BIT(17) )
#define BIT_CGM_51M2_AP_EN              ( BIT(16) )
#define BIT_CGM_64M_AP_EN               ( BIT(15) )
#define BIT_CGM_76M8_AP_EN              ( BIT(14) )
#define BIT_CGM_96M_AP_EN               ( BIT(13) )
#define BIT_CGM_128M_AP_EN              ( BIT(12) )
#define BIT_CGM_153M6_AP_EN             ( BIT(11) )
#define BIT_CGM_192M_AP_EN              ( BIT(10) )
#define BIT_CGM_256M_AP_EN              ( BIT(9) )
#define BIT_CGM_384M_AP_EN              ( BIT(8) )
#define BIT_CGM_312M_AP_EN              ( BIT(7) )
#define BIT_CGM_MPLL_AP_EN              ( BIT(6) )
#define BIT_CGM_WPLL_AP_EN              ( BIT(5) )
#define BIT_CGM_WIFIPLL1_AP_EN          ( BIT(4) )
#define BIT_CGM_TDPLL_AP_EN             ( BIT(3) )
#define BIT_CGM_CPLL_AP_EN              ( BIT(2) )
#define BIT_CGM_DPLL_AP_EN              ( BIT(1) )
#define BIT_CGM_26M_AP_EN               ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_GPU_MM_EN */
#define BIT_CGM_12M_MM_EN               ( BIT(26) )
#define BIT_CGM_24M_MM_EN               ( BIT(25) )
#define BIT_CGM_48M_MM_EN               ( BIT(24) )
#define BIT_CGM_64M_MM_EN               ( BIT(23) )
#define BIT_CGM_76M8_MM_EN              ( BIT(22) )
#define BIT_CGM_96M_MM_EN               ( BIT(21) )
#define BIT_CGM_128M_MM_EN              ( BIT(20) )
#define BIT_CGM_153M6_MM_EN             ( BIT(19) )
#define BIT_CGM_192M_MM_EN              ( BIT(18) )
#define BIT_CGM_256M_MM_EN              ( BIT(17) )
#define BIT_CGM_26M_MM_EN               ( BIT(16) )
#define BIT_CGM_256M_GPU_EN             ( BIT(3) )
#define BIT_CGM_208M_GPU_EN             ( BIT(2) )
#define BIT_CGM_312M_GPU_EN             ( BIT(1) )
#define BIT_CGM_300M_GPU_EN             ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_CP0_EN */
#define BIT_CGM_460M8_CP0W_EN           ( BIT(13) )
#define BIT_CGM_307M2_CP0W_EN           ( BIT(12) )
#define BIT_CGM_51M2_CP0W_EN            ( BIT(11) )
#define BIT_CGM_76M8_CP0W_EN            ( BIT(10) )
#define BIT_CGM_102M4_CP0W_EN           ( BIT(9) )
#define BIT_CGM_192M_CP0_EN             ( BIT(8) )
#define BIT_CGM_51M2_CP0_EN             ( BIT(7) )
#define BIT_CGM_76M8_CP0_EN             ( BIT(6) )
#define BIT_CGM_153M6_CP0_EN            ( BIT(5) )
#define BIT_CGM_48M_CP0_EN              ( BIT(4) )
#define BIT_CGM_64M_CP0_EN              ( BIT(3) )
#define BIT_CGM_96M_CP0_EN              ( BIT(2) )
#define BIT_CGM_128M_CP0_EN             ( BIT(1) )
#define BIT_CGM_26M_CP0_EN              ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_CP1_EN */
#define BIT_CGM_312M_CP1_EN             ( BIT(10) )
#define BIT_CGM_256M_CP1_EN             ( BIT(9) )
#define BIT_CGM_192M_CP1_EN             ( BIT(8) )
#define BIT_CGM_51M2_CP1_EN             ( BIT(7) )
#define BIT_CGM_76M8_CP1_EN             ( BIT(6) )
#define BIT_CGM_153M6_CP1_EN            ( BIT(5) )
#define BIT_CGM_48M_CP1_EN              ( BIT(4) )
#define BIT_CGM_96M_CP1_EN              ( BIT(3) )
#define BIT_CGM_64M_CP1_EN              ( BIT(2) )
#define BIT_CGM_128M_CP1_EN             ( BIT(1) )
#define BIT_CGM_26M_CP1_EN              ( BIT(0) )

/* bits definitions for register REG_PMU_APB_CGM_CP2_EN */
#define BIT_CGM_20M_CP2WF2_EN           ( BIT(11) )
#define BIT_CGM_80M_CP2WF2_EN           ( BIT(10) )
#define BIT_CGM_120M_CP2WF2_EN          ( BIT(9) )
#define BIT_CGM_160M_CP2WF2_EN          ( BIT(8) )
#define BIT_CGM_20M_CP2WF1_EN           ( BIT(7) )
#define BIT_CGM_44M_CP2WF1_EN           ( BIT(6) )
#define BIT_CGM_80M_CP2WF1_EN           ( BIT(5) )
#define BIT_CGM_256M_CP2_EN             ( BIT(4) )
#define BIT_CGM_104M_CP2_EN             ( BIT(3) )
#define BIT_CGM_208M_CP2_EN             ( BIT(2) )
#define BIT_CGM_312M_CP2_EN             ( BIT(1) )
#define BIT_CGM_26M_CP2_EN              ( BIT(0) )

/* bits definitions for register REG_PMU_APB_DDR_OP_MODE_CFG */
#define BIT_DDR_UMCTL_RET_EN            ( BIT(25) )
#define BIT_DDR_PHY_AUTO_RET_EN         ( BIT(24) )
#define BITS_DDR_OPERATE_MODE_CNT_LMT(_x_)( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_DDR_OPERATE_MODE(_x_)      ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_DDR_OPERATE_MODE_IDLE(_x_) ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_PMU_APB_DDR_PHY_RET_CFG */
#define BIT_DDR_PHY_RET_EN              ( BIT(0) )

/* vars definitions for controller REGS_PMU_APB */

#endif //__REGS_PMU_APB_H__
