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


#ifndef __H_REGS_ARM7_AHB_RF_HEADFILE_H__
#define __H_REGS_ARM7_AHB_RF_HEADFILE_H__ __FILE__

#define	REGS_ARM7_AHB_RF

/* registers definitions for ARM7_AHB_RF */
#define REG_ARM7_AHB_RF_ARM7_EB                           SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0000)
#define REG_ARM7_AHB_RF_ARM7_SOFT_RST                     SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0004)
#define REG_ARM7_AHB_RF_AHB_PAUSE                         SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0008)
#define REG_ARM7_AHB_RF_ARM7_SLP_CTL                      SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x000C)



/* bits definitions for register REG_ARM7_AHB_RF_ARM7_EB */
#define BIT_ARM7_GPIO_EB                                  ( BIT(10) )
#define BIT_ARM7_UART_EB                                  ( BIT(9) )
#define BIT_ARM7_TMR_EB                                   ( BIT(8) )
#define BIT_ARM7_SYST_EB                                  ( BIT(7) )
#define BIT_ARM7_WDG_EB                                   ( BIT(6) )
#define BIT_ARM7_EIC_EB                                   ( BIT(5) )
#define BIT_ARM7_INTC_EB                                  ( BIT(4) )
#define BIT_ARM7_IMC_EB                                   ( BIT(2) )
#define BIT_ARM7_TIC_EB                                   ( BIT(1) )
#define BIT_ARM7_DMA_EB                                   ( BIT(0) )

/* bits definitions for register REG_ARM7_AHB_RF_ARM7_SOFT_RST */
#define BIT_ARM7_GPIO_SOFT_RST                            ( BIT(10) )
#define BIT_ARM7_UART_SOFT_RST                            ( BIT(9) )
#define BIT_ARM7_TMR_SOFT_RST                             ( BIT(8) )
#define BIT_ARM7_SYST_SOFT_RST                            ( BIT(7) )
#define BIT_ARM7_WDG_SOFT_RST                             ( BIT(6) )
#define BIT_ARM7_EIC_SOFT_RST                             ( BIT(5) )
#define BIT_ARM7_INTC_SOFT_RST                            ( BIT(4) )
#define BIT_ARM7_IMC_SOFT_RST                             ( BIT(2) )
#define BIT_ARM7_TIC_SOFT_RST                             ( BIT(1) )
#define BIT_ARM7_ARCH_SOFT_RST                            ( BIT(0) )

/* bits definitions for register REG_ARM7_AHB_RF_AHB_PAUSE */
#define BIT_ARM7_DEEP_SLEEP_EN                            ( BIT(2) )
#define BIT_ARM7_SYS_SLEEP_EN                             ( BIT(1) )
#define BIT_ARM7_CORE_SLEEP                               ( BIT(0) )

/* bits definitions for register REG_ARM7_AHB_RF_ARM7_SLP_CTL */
#define BIT_ARM7_SYS_AUTO_GATE_EN                         ( BIT(2) )
#define BIT_ARM7_AHB_AUTO_GATE_EN                         ( BIT(1) )
#define BIT_ARM7_CORE_AUTO_GATE_EN                        ( BIT(0) )

#endif
