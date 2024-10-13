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
#define REG_ARM7_AHB_RF_AP_PROT_CTL                       SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0030)
#define REG_ARM7_AHB_RF_AP_COMM_CTL                       SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0034)
#define REG_ARM7_AHB_RF_CA7_BOOT_VEC                      SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0038)
#define REG_ARM7_AHB_RF_CA7_SDISABLE                      SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x003C)
#define REG_ARM7_AHB_RF_ARM7_RES_REG                      SCI_ADDR(REGS_ARM7_AHB_RF_BASE, 0x0044)



/* bits definitions for register REG_ARM7_AHB_RF_ARM7_EB */
#define BIT_ARM7_GPIO_EB                                  ( BIT(10) )
#define BIT_ARM7_UART_EB                                  ( BIT(9) )
#define BIT_ARM7_TMR_EB                                   ( BIT(8) )
#define BIT_ARM7_SYST_EB                                  ( BIT(7) )
#define BIT_ARM7_WDG_EB                                   ( BIT(6) )
#define BIT_ARM7_EIC_EB                                   ( BIT(5) )
#define BIT_ARM7_INTC_EB                                  ( BIT(4) )
#define BIT_ARM7_EFUSE_EB                                 ( BIT(3) )
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
#define BIT_ARM7_EFUSE_SOFT_RST                           ( BIT(3) )
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

/* bits definitions for register REG_ARM7_AHB_RF_AP_PROT_CTL */
#define BIT_ARM7_CSSYS_SPNIDEN                            ( BIT(22) )
#define BIT_ARM7_CSSYS_SPIDEN                             ( BIT(21) )
#define BIT_ARM7_CSSYS_NIDEN                              ( BIT(20) )
#define BIT_ARM7_CSSYS_DBGEN                              ( BIT(19) )
#define BIT_ARM7_CA7_DAP_DBGEN                            ( BIT(18) )
#define BIT_ARM7_CA7_DAP_SPIDEN                           ( BIT(17) )
#define BIT_ARM7_CA7_DAP_DEVICEEN                         ( BIT(16) )
#define BITS_ARM7_CA7_SPNIDEN(_X_)                        ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_ARM7_CA7_SPIDEN(_X_)                         ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_ARM7_CA7_NIDEN(_X_)                          ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_ARM7_CA7_DBGEN(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ARM7_AHB_RF_AP_COMM_CTL */
#define BIT_ARM7_BOOT_AP_EN                               ( BIT(21) )
#define BITS_ARM7_CA7_L1RSTDISABLE(_X_)                   ( (_X_) << 17 & (BIT(17)|BIT(18)|BIT(19)|BIT(20)) )
#define BIT_ARM7_CA7_L2RSTDISABLE                         ( BIT(16) )
#define BITS_ARM7_CA7_CFGTE(_X_)                          ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_ARM7_CA7_VINITHI(_X_)                        ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_ARM7_AP_AA64NAA32(_X_)                       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ARM7_AHB_RF_CA7_BOOT_VEC */
#define BITS_ARM7_BOOT_AP_VEC(_X_)                        ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ARM7_AHB_RF_CA7_SDISABLE */
#define BIT_ARM7_CA7_CFGSDISABLE                          ( BIT(4) )
#define BITS_ARM7_CA7_CP15SDISABLE(_X_)	                  ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register REG_ARM7_AHB_RF_ARM7_RES_REG */
#define BITS_ARM7_RES_REG_H(_X_)                          ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_ARM7_RES_REG_L(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )

#endif
