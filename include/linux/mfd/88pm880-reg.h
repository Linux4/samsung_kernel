/*
 * Marvell 88PM880 registers
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *  Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MFD_88PM880_REG_H
#define __LINUX_MFD_88PM880_REG_H

#define PM880_BUCK1_VOUT	(0x28)

#define PM880_BUCK1A_VOUT	(0x28) /* voltage 0 */
#define PM880_BUCK1A_1_VOUT	(0x29)
#define PM880_BUCK1A_2_VOUT	(0x2a)
#define PM880_BUCK1A_3_VOUT	(0x2b)
#define PM880_BUCK1A_4_VOUT	(0x2c)
#define PM880_BUCK1A_5_VOUT	(0x2d)
#define PM880_BUCK1A_6_VOUT	(0x2e)
#define PM880_BUCK1A_7_VOUT	(0x2f)
#define PM880_BUCK1A_8_VOUT	(0x30)
#define PM880_BUCK1A_9_VOUT	(0x31)
#define PM880_BUCK1A_10_VOUT	(0x32)
#define PM880_BUCK1A_11_VOUT	(0x33)
#define PM880_BUCK1A_12_VOUT	(0x34)
#define PM880_BUCK1A_13_VOUT	(0x35)
#define PM880_BUCK1A_14_VOUT	(0x36)
#define PM880_BUCK1A_15_VOUT	(0x37)

#define PM880_BUCK1B_VOUT	(0x40)
#define PM880_BUCK1B_1_VOUT	(0x41)
#define PM880_BUCK1B_2_VOUT	(0x42)
#define PM880_BUCK1B_3_VOUT	(0x43)
#define PM880_BUCK1B_4_VOUT	(0x44)
#define PM880_BUCK1B_5_VOUT	(0x45)
#define PM880_BUCK1B_6_VOUT	(0x46)
#define PM880_BUCK1B_7_VOUT	(0x47)
#define PM880_BUCK1B_8_VOUT	(0x48)
#define PM880_BUCK1B_9_VOUT	(0x49)
#define PM880_BUCK1B_10_VOUT	(0x4a)
#define PM880_BUCK1B_11_VOUT	(0x4b)
#define PM880_BUCK1B_12_VOUT	(0x4c)
#define PM880_BUCK1B_13_VOUT	(0x4d)
#define PM880_BUCK1B_14_VOUT	(0x4e)
#define PM880_BUCK1B_15_VOUT	(0x4f)

/* buck7 has dvc function */
#define PM880_BUCK7_VOUT	(0xb8) /* voltage 0 */
#define PM880_BUCK7_1_VOUT	(0xb9)
#define PM880_BUCK7_2_VOUT	(0xba)
#define PM880_BUCK7_3_VOUT	(0xbb)

/*
 * buck sleep mode control registers:
 * 00-disable,
 * 01/10-sleep voltage,
 * 11-active voltage
 */
#define PM880_BUCK1A_SLP_CTRL	(0x27)
#define PM880_BUCK1B_SLP_CTRL	(0x3c)
#define PM880_BUCK2_SLP_CTRL	(0x54)
#define PM880_BUCK3_SLP_CTRL	(0x6c)
/* TODO: there are 7 controls bit for buck4~7 */
#define PM880_BUCK4_SLP_CTRL	(0x84)
#define PM880_BUCK5_SLP_CTRL	(0x94)
#define PM880_BUCK6_SLP_CTRL	(0xa4)
#define PM880_BUCK7_SLP_CTRL	(0xb4)

/*
 * ldo sleep mode control registers:
 * 00-disable,
 * 01/10-sleep voltage,
 * 11-active voltage
 */
#define PM880_LDO1_SLP_CTRL	(0x21)
#define PM880_LDO2_SLP_CTRL	(0x27)
#define PM880_LDO3_SLP_CTRL	(0x2d)
#define PM880_LDO4_SLP_CTRL	(0x33)
#define PM880_LDO5_SLP_CTRL	(0x39)
#define PM880_LDO6_SLP_CTRL	(0x3f)
#define PM880_LDO7_SLP_CTRL	(0x45)
#define PM880_LDO8_SLP_CTRL	(0x4b)
#define PM880_LDO9_SLP_CTRL	(0x51)
#define PM880_LDO10_SLP_CTRL	(0x57)
#define PM880_LDO11_SLP_CTRL	(0x5d)
#define PM880_LDO12_SLP_CTRL	(0x63)
#define PM880_LDO13_SLP_CTRL	(0x69)
#define PM880_LDO14_SLP_CTRL	(0x6f)
#define PM880_LDO15_SLP_CTRL	(0x75)
#define PM880_LDO16_SLP_CTRL	(0x7b)
#define PM880_LDO17_SLP_CTRL	(0x81)
#define PM880_LDO18_SLP_CTRL	(0x87)

#endif /*__LINUX_MFD_88PM880_REG_H */
