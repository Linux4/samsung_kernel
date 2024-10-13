/* the head file modifier:     g   2015-03-26 15:07:27*/

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


#ifndef __H_REGS_AP_APB_HEADFILE_H__
#define __H_REGS_AP_APB_HEADFILE_H__ __FILE__


#define REG_AP_APB_APB_EB         SCI_ADDR(REGS_AP_APB_BASE, 0x0000 )
#define REG_AP_APB_APB_RST        SCI_ADDR(REGS_AP_APB_BASE, 0x0004 )
#define REG_AP_APB_APB_MISC_CTRL  SCI_ADDR(REGS_AP_APB_BASE, 0x0008 )

#define BIT_SPI2_SOFT_RST					(BIT(7))
#define BIT_SPI1_SOFT_RST					(BIT(6))
#define BIT_SPI0_SOFT_RST					(BIT(5))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_APB_APB_EB
// Register Offset : 0x0000
// Description     : 
---------------------------------------------------------------------------*/
#define BIT_AP_APB_CKG_EB                          BIT(19)
#define BIT_AP_CKG_EB                              BIT_AP_APB_CKG_EB
#define BIT_AP_APB_UART4_EB                        BIT(18)
#define BIT_AP_APB_UART3_EB                        BIT(17)
#define BIT_AP_APB_UART2_EB                        BIT(16)
#define BIT_AP_APB_UART1_EB                        BIT(15)
#define BIT_AP_APB_UART0_EB                        BIT(14)
#define BIT_AP_APB_I2C5_EB                         BIT(13)
#define BIT_AP_APB_I2C4_EB                         BIT(12)
#define BIT_AP_APB_I2C3_EB                         BIT(11)
#define BIT_AP_APB_I2C2_EB                         BIT(10)
#define BIT_AP_APB_I2C1_EB                         BIT(9)
#define BIT_AP_APB_I2C0_EB                         BIT(8)
#define BIT_AP_APB_SPI2_EB                         BIT(7)
#define BIT_AP_APB_SPI1_EB                         BIT(6)
#define BIT_AP_APB_SPI0_EB                         BIT(5)
#define BIT_AP_APB_IIS3_EB                         BIT(4)
#define BIT_AP_APB_IIS2_EB                         BIT(3)
#define BIT_AP_APB_IIS1_EB                         BIT(2)
#define BIT_AP_APB_IIS0_EB                         BIT(1)
#define BIT_AP_APB_SIM0_EB                         BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_APB_APB_RST
// Register Offset : 0x0004
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_APB_CKG_SOFT_RST                    BIT(19)
#define BIT_AP_APB_UART4_SOFT_RST                  BIT(18)
#define BIT_AP_APB_UART3_SOFT_RST                  BIT(17)
#define BIT_AP_APB_UART2_SOFT_RST                  BIT(16)
#define BIT_AP_APB_UART1_SOFT_RST                  BIT(15)
#define BIT_AP_APB_UART0_SOFT_RST                  BIT(14)
#define BIT_AP_APB_I2C5_SOFT_RST                   BIT(13)
#define BIT_AP_APB_I2C4_SOFT_RST                   BIT(12)
#define BIT_AP_APB_I2C3_SOFT_RST                   BIT(11)
#define BIT_AP_APB_I2C2_SOFT_RST                   BIT(10)
#define BIT_AP_APB_I2C1_SOFT_RST                   BIT(9)
#define BIT_AP_APB_I2C0_SOFT_RST                   BIT(8)
#define BIT_AP_APB_SPI2_SOFT_RST                   BIT(7)
#define BIT_AP_APB_SPI1_SOFT_RST                   BIT(6)
#define BIT_AP_APB_SPI0_SOFT_RST                   BIT(5)
#define BIT_AP_APB_IIS3_SOFT_RST                   BIT(4)
#define BIT_AP_APB_IIS2_SOFT_RST                   BIT(3)
#define BIT_AP_APB_IIS1_SOFT_RST                   BIT(2)
#define BIT_AP_APB_IIS0_SOFT_RST                   BIT(1)
#define BIT_AP_APB_SIM0_SOFT_RST                   BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_APB_APB_MISC_CTRL
// Register Offset : 0x0008
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AP_APB_SIM_CLK_POLARITY                BIT(1)
#define BIT_AP_APB_FMARK_POLARITY_INV              BIT(0)

#endif
