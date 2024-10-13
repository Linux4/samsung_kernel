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


#ifndef __H_REGS_AP_APB_HEADFILE_H__
#define __H_REGS_AP_APB_HEADFILE_H__ __FILE__

#define	REGS_AP_APB

/* registers definitions for AP_APB */
#define REG_AP_APB_APB_EB                                 SCI_ADDR(REGS_AP_APB_BASE, 0x0000)
#define REG_AP_APB_APB_RST                                SCI_ADDR(REGS_AP_APB_BASE, 0x0004)
#define REG_AP_APB_USB_PHY_TUNE                           SCI_ADDR(REGS_AP_APB_BASE, 0x0008)
#define REG_AP_APB_USB_PHY_TEST                           SCI_ADDR(REGS_AP_APB_BASE, 0x000C)
#define REG_AP_APB_USB_PHY_CTRL                           SCI_ADDR(REGS_AP_APB_BASE, 0x0010)
#define REG_AP_APB_APB_MISC_CTRL                          SCI_ADDR(REGS_AP_APB_BASE, 0x0014)



/* bits definitions for register REG_AP_APB_APB_EB */
#define BIT_INTC3_EB                                      ( BIT(22) )
#define BIT_INTC2_EB                                      ( BIT(21) )
#define BIT_INTC1_EB                                      ( BIT(20) )
#define BIT_INTC0_EB                                      ( BIT(19) )
#define BIT_AP_CKG_EB                                     ( BIT(18) )
#define BIT_UART4_EB                                      ( BIT(17) )
#define BIT_UART3_EB                                      ( BIT(16) )
#define BIT_UART2_EB                                      ( BIT(15) )
#define BIT_UART1_EB                                      ( BIT(14) )
#define BIT_UART0_EB                                      ( BIT(13) )
#define BIT_I2C4_EB                                       ( BIT(12) )
#define BIT_I2C3_EB                                       ( BIT(11) )
#define BIT_I2C2_EB                                       ( BIT(10) )
#define BIT_I2C1_EB                                       ( BIT(9) )
#define BIT_I2C0_EB                                       ( BIT(8) )
#define BIT_SPI2_EB                                       ( BIT(7) )
#define BIT_SPI1_EB                                       ( BIT(6) )
#define BIT_SPI0_EB                                       ( BIT(5) )
#define BIT_IIS3_EB                                       ( BIT(4) )
#define BIT_IIS2_EB                                       ( BIT(3) )
#define BIT_IIS1_EB                                       ( BIT(2) )
#define BIT_IIS0_EB                                       ( BIT(1) )
#define BIT_SIM0_EB                                       ( BIT(0) )

/* bits definitions for register REG_AP_APB_APB_RST */
#define BIT_INTC3_SOFT_RST                                ( BIT(22) )
#define BIT_INTC2_SOFT_RST                                ( BIT(21) )
#define BIT_INTC1_SOFT_RST                                ( BIT(20) )
#define BIT_INTC0_SOFT_RST                                ( BIT(19) )
#define BIT_AP_CKG_SOFT_RST                               ( BIT(18) )
#define BIT_UART4_SOFT_RST                                ( BIT(17) )
#define BIT_UART3_SOFT_RST                                ( BIT(16) )
#define BIT_UART2_SOFT_RST                                ( BIT(15) )
#define BIT_UART1_SOFT_RST                                ( BIT(14) )
#define BIT_UART0_SOFT_RST                                ( BIT(13) )
#define BIT_I2C4_SOFT_RST                                 ( BIT(12) )
#define BIT_I2C3_SOFT_RST                                 ( BIT(11) )
#define BIT_I2C2_SOFT_RST                                 ( BIT(10) )
#define BIT_I2C1_SOFT_RST                                 ( BIT(9) )
#define BIT_I2C0_SOFT_RST                                 ( BIT(8) )
#define BIT_SPI2_SOFT_RST                                 ( BIT(7) )
#define BIT_SPI1_SOFT_RST                                 ( BIT(6) )
#define BIT_SPI0_SOFT_RST                                 ( BIT(5) )
#define BIT_IIS3_SOFT_RST                                 ( BIT(4) )
#define BIT_IIS2_SOFT_RST                                 ( BIT(3) )
#define BIT_IIS1_SOFT_RST                                 ( BIT(2) )
#define BIT_IIS0_SOFT_RST                                 ( BIT(1) )
#define BIT_SIM0_SOFT_RST                                 ( BIT(0) )

/* bits definitions for register REG_AP_APB_USB_PHY_TUNE */
#define BITS_OTGTUNE(_X_)                                 ( (_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)) )
#define BITS_COMPDISTUNE(_X_)                             ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_TXPREEMPPULSETUNE                             ( BIT(20) )
#define BITS_TXRESTUNE(_X_)                               ( (_X_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_TXHSXVTUNE(_X_)                              ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_TXVREFTUNE(_X_)                              ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_TXPREEMPAMPTUNE(_X_)                         ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_TXRISETUNE(_X_)                              ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_TXFSLSTUNE(_X_)                              ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_SQRXTUNE(_X_)                                ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_APB_USB_PHY_TEST */
#define BIT_ATERESET                                      ( BIT(31) )
#define BIT_VBUS_VALID_EXT_SEL                            ( BIT(26) )
#define BIT_VBUS_VALID_EXT                                ( BIT(25) )
#define BIT_OTGDISABLE                                    ( BIT(24) )
#define BIT_TESTBURNIN                                    ( BIT(21) )
#define BIT_LOOPBACKENB                                   ( BIT(20) )
#define BITS_TESTDATAOUT(_X_)                             ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_VATESTENB(_X_)                               ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BIT_TESTCLK                                       ( BIT(13) )
#define BIT_TESTDATAOUTSEL                                ( BIT(12) )
#define BITS_TESTADDR(_X_)                                ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_TESTDATAIN(_X_)                              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AP_APB_USB_PHY_CTRL */
#define BITS_SS_SCALEDOWNMODE(_X_)                        ( (_X_) << 25 & (BIT(25)|BIT(26)) )
#define BIT_TXBITSTUFFENH                                 ( BIT(23) )
#define BIT_TXBITSTUFFEN                                  ( BIT(22) )
#define BIT_DMPULLDOWN                                    ( BIT(21) )
#define BIT_DPPULLDOWN                                    ( BIT(20) )
#define BIT_DMPULLUP                                      ( BIT(9) )
#define BIT_COMMONONN                                     ( BIT(8) )
#define BITS_REFCLKSEL(_X_)                               ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_FSEL(_X_)                                    ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AP_APB_APB_MISC_CTRL */
#define BIT_SIM_CLK_POLARITY                              ( BIT(1) )
#define BIT_FMARK_POLARITY_INV                            ( BIT(0) )

#endif
