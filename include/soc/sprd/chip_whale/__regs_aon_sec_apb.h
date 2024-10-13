/* the head file modifier:     g   2015-03-26 15:05:02*/

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


#ifndef __H_REGS_AON_SEC_APB_HEADFILE_H__
#define __H_REGS_AON_SEC_APB_HEADFILE_H__ __FILE__



#define REG_AON_SEC_APB_CHIP_KPRTL_0          SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0000 )
#define REG_AON_SEC_APB_CHIP_KPRTL_1          SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0004 )
#define REG_AON_SEC_APB_CHIP_KPRTL_2          SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0008 )
#define REG_AON_SEC_APB_CHIP_KPRTL_3          SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x000C )
#define REG_AON_SEC_APB_SEC_EB                SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0010 )
#define REG_AON_SEC_APB_SEC_SOFT_RST          SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0014 )
#define REG_AON_SEC_APB_CA53_CFG_CTRL         SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0018 )
#define REG_AON_SEC_APB_CA53_SOFT_RST         SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x001C )
#define REG_AON_SEC_APB_CA53_LIT_CLK_CFG      SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0020 )
#define REG_AON_SEC_APB_CA53_BIG_CLK_CFG      SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0024 )
#define REG_AON_SEC_APB_CA53_TOP_CLK_CFG      SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0028 )
#define REG_AON_SEC_APB_CCI_CFG0              SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x002C )
#define REG_AON_SEC_APB_CCI_CFG1              SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0030 )
#define REG_AON_SEC_APB_RVBARADDR0_LIT        SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0034 )
#define REG_AON_SEC_APB_RVBARADDR1_LIT        SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0038 )
#define REG_AON_SEC_APB_RVBARADDR2_LIT        SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x003C )
#define REG_AON_SEC_APB_RVBARADDR3_LIT        SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0040 )
#define REG_AON_SEC_APB_RVBARADDR0_BIG        SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0044 )
#define REG_AON_SEC_APB_RVBARADDR1_BIG        SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0048 )
#define REG_AON_SEC_APB_RVBARADDR2_BIG        SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x004C )
#define REG_AON_SEC_APB_RVBARADDR3_BIG        SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0050 )
#define REG_AON_SEC_APB_CA53_NOC_CTRL         SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0054 )
#define REG_AON_SEC_APB_AP_NOC_CTRL           SCI_ADDR(REGS_AON_SEC_APB_BASE, 0x0058 )

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CHIP_KPRTL_0
// Register Offset : 0x0000
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_KPRTL_0(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CHIP_KPRTL_1
// Register Offset : 0x0004
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_KPRTL_1(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CHIP_KPRTL_2
// Register Offset : 0x0008
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_KPRTL_2(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CHIP_KPRTL_3
// Register Offset : 0x000C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_KPRTL_3(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_SEC_EB
// Register Offset : 0x0010
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_SEC_RTC_CLK_GATE_EB                      BIT(7)
#define BIT_AON_SEC_APB_SEC_GPIO_EB                              BIT(6)
#define BIT_AON_SEC_APB_SEC_WDG_EB                               BIT(5)
#define BIT_AON_SEC_APB_SEC_WDG_RTC_EB                           BIT(4)
#define BIT_AON_SEC_APB_SEC_RTC_EB                               BIT(3)
#define BIT_AON_SEC_APB_SEC_TMR0_EB                              BIT(2)
#define BIT_AON_SEC_APB_SEC_TMR0_RTC_EB                          BIT(1)
#define BIT_AON_SEC_APB_SEC_TZPC_EB                              BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_SEC_SOFT_RST
// Register Offset : 0x0014
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_SEC_GPIO_SOFT_RST                        BIT(4)
#define BIT_AON_SEC_APB_SEC_WDG_SOFT_RST                         BIT(3)
#define BIT_AON_SEC_APB_SEC_RTC_SOFT_RST                         BIT(2)
#define BIT_AON_SEC_APB_SEC_TMR0_SOFT_RST                        BIT(1)
#define BIT_AON_SEC_APB_SEC_TZPC_SOFT_RST                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CA53_CFG_CTRL
// Register Offset : 0x0018
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_CA53_GIC_CFGSDISABLE                     BIT(8)
#define BIT_AON_SEC_APB_CA53_BIG_CP15SDISABLE(x)                 (((x) & 0xF) << 4)
#define BIT_AON_SEC_APB_CA53_LIT_CP15SDISABLE(x)                 (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CA53_SOFT_RST
// Register Offset : 0x001C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_CA53_BIG_DBG_SOFT_RST                    BIT(25)
#define BIT_AON_SEC_APB_CA53_BIG_L2_SOFT_RST                     BIT(24)
#define BIT_AON_SEC_APB_CA53_BIG_ATB_SOFT_RST(x)                 (((x) & 0xF) << 20)
#define BIT_AON_SEC_APB_CA53_BIG_CORE_SOFT_RST(x)                (((x) & 0xF) << 16)
#define BIT_AON_SEC_APB_CA53_LIT_DBG_SOFT_RST                    BIT(9)
#define BIT_AON_SEC_APB_CA53_LIT_L2_SOFT_RST                     BIT(8)
#define BIT_AON_SEC_APB_CA53_LIT_ATB_SOFT_RST(x)                 (((x) & 0xF) << 4)
#define BIT_AON_SEC_APB_CA53_LIT_CORE_SOFT_RST(x)                (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CA53_LIT_CLK_CFG
// Register Offset : 0x0020
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_CLK_CA53_LIT_ATB_DIV(x)                  (((x) & 0x7) << 16)
#define BIT_AON_SEC_APB_CLK_CA53_LIT_DBG_DIV(x)                  (((x) & 0x7) << 12)
#define BIT_AON_SEC_APB_CLK_CA53_LIT_ACE_DIV(x)                  (((x) & 0x7) << 8)
#define BIT_AON_SEC_APB_CLK_CA53_LIT_MCU_DIV(x)                  (((x) & 0x7) << 4)
#define BIT_AON_SEC_APB_CLK_CA53_LIT_MPLL0_SEL                   BIT(3)
#define BIT_AON_SEC_APB_CLK_CA53_LIT_MCU_SEL(x)                  (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CA53_BIG_CLK_CFG
// Register Offset : 0x0024
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_CLK_CA53_BIG_ATB_DIV(x)                  (((x) & 0x7) << 16)
#define BIT_AON_SEC_APB_CLK_CA53_BIG_DBG_DIV(x)                  (((x) & 0x7) << 12)
#define BIT_AON_SEC_APB_CLK_CA53_BIG_ACE_DIV(x)                  (((x) & 0x7) << 8)
#define BIT_AON_SEC_APB_CLK_CA53_BIG_MCU_DIV(x)                  (((x) & 0x7) << 4)
#define BIT_AON_SEC_APB_CLK_CA53_BIG_MPLL1_SEL                   BIT(3)
#define BIT_AON_SEC_APB_CLK_CA53_BIG_MCU_SEL(x)                  (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CA53_TOP_CLK_CFG
// Register Offset : 0x0028
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_CLK_GIC_DIV(x)                           (((x) & 0x3) << 18)
#define BIT_AON_SEC_APB_CLK_GIC_SEL(x)                           (((x) & 0x3) << 16)
#define BIT_AON_SEC_APB_CLK_CSSYS_DIV(x)                         (((x) & 0x3) << 10)
#define BIT_AON_SEC_APB_CLK_CSSYS_SEL(x)                         (((x) & 0x3) << 8)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CCI_CFG0
// Register Offset : 0x002C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_CCI_PERIPHBASE_H25(x)                    (((x) & 0x1FFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CCI_CFG1
// Register Offset : 0x0030
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_CCI_AWQOS_BIG(x)                         (((x) & 0xF) << 28)
#define BIT_AON_SEC_APB_CCI_ARQOS_BIG(x)                         (((x) & 0xF) << 24)
#define BIT_AON_SEC_APB_CCI_AWQOS_LIT(x)                         (((x) & 0xF) << 20)
#define BIT_AON_SEC_APB_CCI_ARQOS_LIT(x)                         (((x) & 0xF) << 16)
#define BIT_AON_SEC_APB_CCI_QOSOVERRIDE(x)                       (((x) & 0x1F) << 3)
#define BIT_AON_SEC_APB_CCI_BUFFERABLEOVERRIDE(x)                (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_RVBARADDR0_LIT
// Register Offset : 0x0034
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_RVBARADDR0_LIT(x)                        (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_RVBARADDR1_LIT
// Register Offset : 0x0038
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_RVBARADDR1_LIT(x)                        (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_RVBARADDR2_LIT
// Register Offset : 0x003C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_RVBARADDR2_LIT(x)                        (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_RVBARADDR3_LIT
// Register Offset : 0x0040
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_RVBARADDR3_LIT(x)                        (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_RVBARADDR0_BIG
// Register Offset : 0x0044
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_RVBARADDR0_BIG(x)                        (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_RVBARADDR1_BIG
// Register Offset : 0x0048
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_RVBARADDR1_BIG(x)                        (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_RVBARADDR2_BIG
// Register Offset : 0x004C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_RVBARADDR2_BIG(x)                        (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_RVBARADDR3_BIG
// Register Offset : 0x0050
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_RVBARADDR3_BIG(x)                        (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_CA53_NOC_CTRL
// Register Offset : 0x0054
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_CHIP_DDR_CAPACITY_I(x)                   (((x) & 0x7) << 9)
#define BIT_AON_SEC_APB_CA53_M0_IDLE                             BIT(8)
#define BIT_AON_SEC_APB_CA53_SERVICE_ACCESS_EN                   BIT(3)
#define BIT_AON_SEC_APB_CA53_INTERLEAVE_MODE(x)                  (((x) & 0x3) << 1)
#define BIT_AON_SEC_APB_CA53_INTERLEAVE_SEL                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_SEC_APB_AP_NOC_CTRL
// Register Offset : 0x0058
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_SEC_APB_AP_M0_IDLE                               BIT(8)
#define BIT_AON_SEC_APB_AP_SERVICE_ACCESS_EN                     BIT(3)
#define BIT_AON_SEC_APB_AP_INTERLEAVE_MODE(x)                    (((x) & 0x3) << 1)
#define BIT_AON_SEC_APB_AP_INTERLEAVE_SEL                        BIT(0)

#endif
