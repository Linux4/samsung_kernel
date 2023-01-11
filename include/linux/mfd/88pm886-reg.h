/*
 * Marvell 88PM886 registers
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *  Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MFD_88PM886_REG_H
#define __LINUX_MFD_88PM886_REG_H

#define PM886_BUCK1_VOUT	(0xa5)
#define PM886_BUCK1_1_VOUT	(0xa6)
#define PM886_BUCK1_2_VOUT	(0xa7)
#define PM886_BUCK1_3_VOUT	(0xa8)
#define PM886_BUCK1_4_VOUT	(0x9a)
#define PM886_BUCK1_5_VOUT	(0x9b)
#define PM886_BUCK1_6_VOUT	(0x9c)
#define PM886_BUCK1_7_VOUT	(0x9d)

/*
 * buck sleep mode control registers:
 * 00-disable,
 * 01/10-sleep voltage,
 * 11-active voltage
 */
#define PM886_BUCK1_SLP_CTRL	(0xa2)
#define PM886_BUCK2_SLP_CTRL	(0xb0)
#define PM886_BUCK3_SLP_CTRL	(0xbe)
#define PM886_BUCK4_SLP_CTRL	(0xcc)
#define PM886_BUCK5_SLP_CTRL	(0xda)

/*
 * ldo sleep mode control registers:
 * 00-disable,
 * 01/10-sleep voltage,
 * 11-active voltage
 */
#define PM886_LDO1_SLP_CTRL	(0x21)
#define PM886_LDO2_SLP_CTRL	(0x27)
#define PM886_LDO3_SLP_CTRL	(0x2d)
#define PM886_LDO4_SLP_CTRL	(0x33)
#define PM886_LDO5_SLP_CTRL	(0x39)
#define PM886_LDO6_SLP_CTRL	(0x3f)
#define PM886_LDO7_SLP_CTRL	(0x45)
#define PM886_LDO8_SLP_CTRL	(0x4b)
#define PM886_LDO9_SLP_CTRL	(0x51)
#define PM886_LDO10_SLP_CTRL	(0x57)
#define PM886_LDO11_SLP_CTRL	(0x5d)
#define PM886_LDO12_SLP_CTRL	(0x63)
#define PM886_LDO13_SLP_CTRL	(0x69)
#define PM886_LDO14_SLP_CTRL	(0x6f)
#define PM886_LDO15_SLP_CTRL	(0x75)
#define PM886_LDO16_SLP_CTRL	(0x7b)

#endif
