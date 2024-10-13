/* the head file modifier:     g   2015-03-26 16:12:24*/

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


#ifndef __H_REGS_DDR_PREDIV_HEADFILE_H__
#define __H_REGS_DDR_PREDIV_HEADFILE_H__ __FILE__


/* registers definitions for DDR_PREDIV */
#define REG_DDR_PREDIV_SOFT_CNT_DONE0_CFG    SCI_ADDR(REGS_DDR_PREDIV_BASE, 0x0020 )
#define REG_DDR_PREDIV_PLL_WAIT_SEL0_CFG     SCI_ADDR(REGS_DDR_PREDIV_BASE, 0x0024 )
#define REG_DDR_PREDIV_PLL_WAIT_SW_CTL0_CFG  SCI_ADDR(REGS_DDR_PREDIV_BASE, 0x0028 )
#define REG_DDR_PREDIV_DIV_EN_SEL0_CFG       SCI_ADDR(REGS_DDR_PREDIV_BASE, 0x002C )
#define REG_DDR_PREDIV_DIV_EN_SW_CTL0_CFG    SCI_ADDR(REGS_DDR_PREDIV_BASE, 0x0030 )
#define REG_DDR_PREDIV_GATE_EN_SEL0_CFG      SCI_ADDR(REGS_DDR_PREDIV_BASE, 0x0034 )
#define REG_DDR_PREDIV_GATE_EN_SW_CTL0_CFG   SCI_ADDR(REGS_DDR_PREDIV_BASE, 0x0038 )

/*---------------------------------------------------------------------------
// Register Name   : REG_DDR_PREDIV_SOFT_CNT_DONE0_CFG
// Register Offset : 0x0020
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DDR_PREDIV_DPLL_1600M_SOFT_CNT_DONE                          BIT(5)
#define BIT_DDR_PREDIV_DPLL_800M_SOFT_CNT_DONE                           BIT(4)
#define BIT_DDR_PREDIV_DPLL_533M_SOFT_CNT_DONE                           BIT(3)
#define BIT_DDR_PREDIV_DPLL_400M_SOFT_CNT_DONE                           BIT(2)
#define BIT_DDR_PREDIV_DPLL_320M_SOFT_CNT_DONE                           BIT(1)
#define BIT_DDR_PREDIV_DPLL_228M_SOFT_CNT_DONE                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_DDR_PREDIV_PLL_WAIT_SEL0_CFG
// Register Offset : 0x0024
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DDR_PREDIV_DPLL_1600M_WAIT_AUTO_GATE_SEL                     BIT(5)
#define BIT_DDR_PREDIV_DPLL_800M_WAIT_AUTO_GATE_SEL                      BIT(4)
#define BIT_DDR_PREDIV_DPLL_533M_WAIT_AUTO_GATE_SEL                      BIT(3)
#define BIT_DDR_PREDIV_DPLL_400M_WAIT_AUTO_GATE_SEL                      BIT(2)
#define BIT_DDR_PREDIV_DPLL_320M_WAIT_AUTO_GATE_SEL                      BIT(1)
#define BIT_DDR_PREDIV_DPLL_228M_WAIT_AUTO_GATE_SEL                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_DDR_PREDIV_PLL_WAIT_SW_CTL0_CFG
// Register Offset : 0x0028
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DDR_PREDIV_DPLL_1600M_WAIT_FORCE_EN                          BIT(5)
#define BIT_DDR_PREDIV_DPLL_800M_WAIT_FORCE_EN                           BIT(4)
#define BIT_DDR_PREDIV_DPLL_533M_WAIT_FORCE_EN                           BIT(3)
#define BIT_DDR_PREDIV_DPLL_400M_WAIT_FORCE_EN                           BIT(2)
#define BIT_DDR_PREDIV_DPLL_320M_WAIT_FORCE_EN                           BIT(1)
#define BIT_DDR_PREDIV_DPLL_228M_WAIT_FORCE_EN                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_DDR_PREDIV_DIV_EN_SEL0_CFG
// Register Offset : 0x002C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DDR_PREDIV_DPLL_DIV_400M_50M_AUTO_GATE_SEL                   BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_DDR_PREDIV_DIV_EN_SW_CTL0_CFG
// Register Offset : 0x0030
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DDR_PREDIV_DPLL_DIV_400M_50M_FORCE_EN                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_DDR_PREDIV_GATE_EN_SEL0_CFG
// Register Offset : 0x0034
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DDR_PREDIV_CGM_DPLL_400M_VSP_AUTO_GATE_SEL                   BIT(9)
#define BIT_DDR_PREDIV_CGM_DPLL_400M_CAM_AUTO_GATE_SEL                   BIT(8)
#define BIT_DDR_PREDIV_CGM_DPLL_400M_DISP_AUTO_GATE_SEL                  BIT(7)
#define BIT_DDR_PREDIV_CGM_DPLL_1600M_AON_AUTO_GATE_SEL                  BIT(6)
#define BIT_DDR_PREDIV_CGM_DPLL_800M_AON_AUTO_GATE_SEL                   BIT(5)
#define BIT_DDR_PREDIV_CGM_DPLL_533M_AON_AUTO_GATE_SEL                   BIT(4)
#define BIT_DDR_PREDIV_CGM_DPLL_400M_AON_AUTO_GATE_SEL                   BIT(3)
#define BIT_DDR_PREDIV_CGM_DPLL_320M_AON_AUTO_GATE_SEL                   BIT(2)
#define BIT_DDR_PREDIV_CGM_DPLL_228M_AON_AUTO_GATE_SEL                   BIT(1)
#define BIT_DDR_PREDIV_CGM_DPLL_50M_AON_AUTO_GATE_SEL                    BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_DDR_PREDIV_GATE_EN_SW_CTL0_CFG
// Register Offset : 0x0038
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_DDR_PREDIV_CGM_DPLL_400M_VSP_FORCE_EN                        BIT(9)
#define BIT_DDR_PREDIV_CGM_DPLL_400M_CAM_FORCE_EN                        BIT(8)
#define BIT_DDR_PREDIV_CGM_DPLL_400M_DISP_FORCE_EN                       BIT(7)
#define BIT_DDR_PREDIV_CGM_DPLL_1600M_AON_FORCE_EN                       BIT(6)
#define BIT_DDR_PREDIV_CGM_DPLL_800M_AON_FORCE_EN                        BIT(5)
#define BIT_DDR_PREDIV_CGM_DPLL_533M_AON_FORCE_EN                        BIT(4)
#define BIT_DDR_PREDIV_CGM_DPLL_400M_AON_FORCE_EN                        BIT(3)
#define BIT_DDR_PREDIV_CGM_DPLL_320M_AON_FORCE_EN                        BIT(2)
#define BIT_DDR_PREDIV_CGM_DPLL_228M_AON_FORCE_EN                        BIT(1)
#define BIT_DDR_PREDIV_CGM_DPLL_50M_AON_FORCE_EN                         BIT(0)

#endif
