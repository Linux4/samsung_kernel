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


#ifndef __H_REGS_WCDMA_APB_RF_HEADFILE_H__
#define __H_REGS_WCDMA_APB_RF_HEADFILE_H__ __FILE__

#define	REGS_WCDMA_APB_RF

/* registers definitions for WCDMA_APB_RF */
#define REG_WCDMA_APB_RF_APB_EB                           SCI_ADDR(REGS_WCDMA_APB_RF_BASE, 0x0000)
#define REG_WCDMA_APB_RF_APB_RST                          SCI_ADDR(REGS_WCDMA_APB_RF_BASE, 0x0004)
#define REG_WCDMA_APB_RF_APB_SLEEP_ST                     SCI_ADDR(REGS_WCDMA_APB_RF_BASE, 0x0008)
#define REG_WCDMA_APB_RF_APB_MCU_RST                      SCI_ADDR(REGS_WCDMA_APB_RF_BASE, 0x0018)
#define REG_WCDMA_APB_RF_APB_CLK_SEL0                     SCI_ADDR(REGS_WCDMA_APB_RF_BASE, 0x001C)
#define REG_WCDMA_APB_RF_APB_CLK_DIV0                     SCI_ADDR(REGS_WCDMA_APB_RF_BASE, 0x0020)
#define REG_WCDMA_APB_RF_APB_ARCH_EB                      SCI_ADDR(REGS_WCDMA_APB_RF_BASE, 0x0024)
#define REG_WCDMA_APB_RF_APB_SLP_CTL                      SCI_ADDR(REGS_WCDMA_APB_RF_BASE, 0x0028)



/* bits definitions for register REG_WCDMA_APB_RF_APB_EB */
#define BIT_TMR_RTC_EB                                    ( BIT(6) )
#define BIT_TMR_EB                                        ( BIT(5) )
#define BIT_SYSTMR_RTC_EB                                 ( BIT(4) )
#define BIT_SYSTMR_EB                                     ( BIT(3) )
#define BIT_UART0_EB                                      ( BIT(2) )
#define BIT_WDG_RTC_EB                                    ( BIT(1) )
#define BIT_WDG_EB                                        ( BIT(0) )

/* bits definitions for register REG_WCDMA_APB_RF_APB_RST */
#define BIT_TMR_SOFT_RST                                  ( BIT(3) )
#define BIT_SYSTMR_SOFT_RST                               ( BIT(2) )
#define BIT_UART0_SOFT_RST                                ( BIT(1) )
#define BIT_WDG_SOFT_RST                                  ( BIT(0) )

/* bits definitions for register REG_WCDMA_APB_RF_APB_SLEEP_ST */
#define BIT_CHIP_SLEEP_REC_CLR                            ( BIT(1) )
#define BIT_CHIP_SLEEP_REC                                ( BIT(0) )

/* bits definitions for register REG_WCDMA_APB_RF_APB_MCU_RST */
#define BIT_MCU_SOFT_RST_SET                              ( BIT(0) )

/* bits definitions for register REG_WCDMA_APB_RF_APB_CLK_SEL0 */
#define BITS_CLK_UART0_SEL(_X_)                           ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_WCDMA_APB_RF_APB_CLK_DIV0 */
#define BITS_CLK_UART0_DIV(_X_)                           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_WCDMA_APB_RF_APB_ARCH_EB */
#define BIT_RTC_ARCH_EB                                   ( BIT(1) )
#define BIT_APB_ARCH_EB                                   ( BIT(0) )

/* bits definitions for register REG_WCDMA_APB_RF_APB_SLP_CTL */
#define BIT_APB_PERI_FRC_ON                               ( BIT(1) )
#define BIT_APB_PERI_FRC_SLP                              ( BIT(0) )

#endif
