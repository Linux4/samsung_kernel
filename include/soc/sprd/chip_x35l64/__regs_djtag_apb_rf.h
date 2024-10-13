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


#ifndef __H_REGS_DJTAG_APB_RF_HEADFILE_H__
#define __H_REGS_DJTAG_APB_RF_HEADFILE_H__ __FILE__

#define	REGS_DJTAG_APB_RF

/* registers definitions for DJTAG_APB_RF */
#define REG_DJTAG_APB_RF_DJTAG_IR_LEN_CFG                 SCI_ADDR(REGS_DJTAG_APB_RF_BASE, 0x0000)
#define REG_DJTAG_APB_RF_DJTAG_DR_LEN_CFG                 SCI_ADDR(REGS_DJTAG_APB_RF_BASE, 0x0004)
#define REG_DJTAG_APB_RF_DJTAG_IR_CFG                     SCI_ADDR(REGS_DJTAG_APB_RF_BASE, 0x0008)
#define REG_DJTAG_APB_RF_DJTAG_DR_CFG                     SCI_ADDR(REGS_DJTAG_APB_RF_BASE, 0x000C)
#define REG_DJTAG_APB_RF_DR_PAUSE_RECOV_CFG               SCI_ADDR(REGS_DJTAG_APB_RF_BASE, 0x0010)
#define REG_DJTAG_APB_RF_DJTAG_RND_EN_CFG                 SCI_ADDR(REGS_DJTAG_APB_RF_BASE, 0x0014)
#define REG_DJTAG_APB_RF_DJTAG_UPD_DR_CFG                 SCI_ADDR(REGS_DJTAG_APB_RF_BASE, 0x0018)



/* bits definitions for register REG_DJTAG_APB_RF_DJTAG_IR_LEN_CFG */
#define BITS_IR_LEN(_X_)                                  ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_DJTAG_APB_RF_DJTAG_DR_LEN_CFG */
#define BITS_DR_LEN(_X_)                                  ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_DJTAG_APB_RF_DJTAG_IR_CFG */
#define BITS_DJTAG_IR(_X_)                                (_X_)

/* bits definitions for register REG_DJTAG_APB_RF_DJTAG_DR_CFG */
#define BITS_DJTAG_DR(_X_)                                (_X_)

/* bits definitions for register REG_DJTAG_APB_RF_DR_PAUSE_RECOV_CFG */
#define BIT_DR_PAUSE_RECOV                                ( BIT(0) )

/* bits definitions for register REG_DJTAG_APB_RF_DJTAG_RND_EN_CFG */
#define BIT_DJTAG_RND_EN                                  ( BIT(0) )

/* bits definitions for register REG_DJTAG_APB_RF_DJTAG_UPD_DR_CFG */
#define BITS_DJTAG_UPD_DR(_X_)                            (_X_)

#endif
