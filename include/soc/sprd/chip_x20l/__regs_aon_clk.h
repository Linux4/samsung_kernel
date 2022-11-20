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

#ifndef __REGS_AON_CLK_H__
#define __REGS_AON_CLK_H__

#define REGS_AON_CLK

/* registers definitions for controller REGS_AON_CLK */
#define REG_AON_CLK_PUB_AHB_CFG         SCI_ADDR(SPRD_AONCKG_BASE, 0x0020)
#define REG_AON_CLK_AON_APB_CFG         SCI_ADDR(SPRD_AONCKG_BASE, 0x0024)
#define REG_AON_CLK_ADI_CFG             SCI_ADDR(SPRD_AONCKG_BASE, 0x0028)
#define REG_AON_CLK_PWM0_CFG            SCI_ADDR(SPRD_AONCKG_BASE, 0x002c)
#define REG_AON_CLK_PWM1_CFG            SCI_ADDR(SPRD_AONCKG_BASE, 0x0030)
#define REG_AON_CLK_PWM2_CFG            SCI_ADDR(SPRD_AONCKG_BASE, 0x0034)
#define REG_AON_CLK_PWM3_CFG            SCI_ADDR(SPRD_AONCKG_BASE, 0x0038)
#define REG_AON_CLK_ARM7_UART_CFG       SCI_ADDR(SPRD_AONCKG_BASE, 0x003c)
#define REG_AON_CLK_THM_CFG             SCI_ADDR(SPRD_AONCKG_BASE, 0x0040)
#define REG_AON_CLK_MSPI_CFG            SCI_ADDR(SPRD_AONCKG_BASE, 0x0044)
#define REG_AON_CLK_I2C_CFG             SCI_ADDR(SPRD_AONCKG_BASE, 0x0048)
#define REG_AON_CLK_AVS_CFG             SCI_ADDR(SPRD_AONCKG_BASE, 0x004c)
#define REG_AON_CLK_AUDIF_CFG           SCI_ADDR(SPRD_AONCKG_BASE, 0x0050)
#define REG_AON_CLK_CA7_DAP_CFG         SCI_ADDR(SPRD_AONCKG_BASE, 0x0054)
#define REG_AON_CLK_EMC_CFG             SCI_ADDR(SPRD_AONCKG_BASE, 0x0058)
#define REG_AON_CLK_CA7_TS_CFG          SCI_ADDR(SPRD_AONCKG_BASE, 0x005c)
#define REG_AON_CLK_DJTAG_CLK_CFG       SCI_ADDR(SPRD_AONCKG_BASE, 0x0060)
#define REG_AON_CLK_ARM7_AHB_CFG        SCI_ADDR(SPRD_AONCKG_BASE, 0x0064)
#define REG_AON_CLK_CA5_TS_CFG          SCI_ADDR(SPRD_AONCKG_BASE, 0x0068)
/* vars definitions for controller REGS_AON_CLK */
#define PUB_AHB_CLK_SEL_SHIFT		(0x0)
#define PUB_AHB_CLK_SEL_MASK		(0x3 << PUB_AHB_CLK_SEL_SHIFT)

#define PUB_APB_CLK_SEL_SHIFT		(0x0)
#define PUB_APB_CLK_SEL_MASK		(0x3 << PUB_AHB_CLK_SEL_SHIFT)
#endif //__REGS_AON_CLK_H__
