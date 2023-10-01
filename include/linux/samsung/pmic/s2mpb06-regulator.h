/*
 * s2mpb06-regulator.h - Header for Samsung s2mpb06 regulator driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef __S2MPB06_MFD_REGULATOR_H__
#define __S2MPB06_MFD_REGULATOR_H__

/* PMIC ADDRESS: S2MPB06 EVT1 Registar Map */
#define S2MPB06_REG_PMIC_ID			0x00
#define S2MPB06_REG_LDO1_CTRL			0x11
#define S2MPB06_REG_LDO2_CTRL			0x12
#define S2MPB06_REG_LDO3_CTRL			0x13
#define S2MPB06_REG_LDO4_CTRL			0x14
#define S2MPB06_REG_LDO5_CTRL			0x15
#define S2MPB06_REG_LDO6_CTRL			0x16
#define S2MPB06_REG_LDO7_CTRL			0x17
#define S2MPB06_REG_OCP_STATUS			0x18
#define S2MPB06_REG_UVLO_TSD_STATUS		0x19
#define S2MPB06_REG_POK_STATUS			0x1A
#define S2MPB06_REG_LDO1_OUTPUT			0x21
#define S2MPB06_REG_LDO2_OUTPUT			0x22
#define S2MPB06_REG_LDO3_OUTPUT			0x23
#define S2MPB06_REG_LDO4_OUTPUT			0x24
#define S2MPB06_REG_LDO5_OUTPUT			0x25
#define S2MPB06_REG_LDO6_OUTPUT			0x26
#define S2MPB06_REG_LDO7_OUTPUT			0x27
#define S2MPB06_REG_LDO_DSCH			0x28
#define S2MPB06_REG_OCP_ON			0x29
#define S2MPB06_REG_OCP_INT			0x2A
#define S2MPB06_REG_OCP_LDO_OFF			0x2B
#define S2MPB06_REG_TSD_CTRL			0x2C
#define S2MPB06_REG_LDO4_7_SS_CTRL		0x2D
#define S2MPB06_REG_LDO1_3_SS_CTRL		0x2E
#define S2MPB06_REG_UVLO_CTRL			0x2F
#define S2MPB06_REG_SPECIAL_REG_ACCESS		0x80
#define S2MPB06_REG_LDO3_SPECIAL_REG		0xC9
#define S2MPB06_REG_LDO4_SPECIAL_REG		0xCA
#define S2MPB06_REG_LDO5_SPECIAL_REG		0xCB
#define S2MPB06_REG_LDO6_SPECIAL_REG		0xCC
#define S2MPB06_REG_LDO7_SPECIAL_REG		0xCD

/* S2MPB06 regulator ids */
enum S2MPB06_regulators {
	S2MPB06_LDO1,
	S2MPB06_LDO2,
	S2MPB06_LDO3,
	S2MPB06_LDO4,
	S2MPB06_LDO5,
	S2MPB06_LDO6,
	S2MPB06_LDO7,

	S2MPB06_REGULATOR_MAX,
};

/* LDO 1,2 */
#define S2MPB06_LDO_MIN1	525000
#define S2MPB06_LDO_STEP1	5000
/* LDO 3,4,5,6,7 */
#define S2MPB06_LDO_MIN2	1050000
#define S2MPB06_LDO_STEP2	10000

#define S2MPB06_LDO_VSEL_MASK	0xFF
#define S2MPB06_LDO_ENABLE_MASK	0x10

#define S2MPB06_RAMP_DELAY		12000
#define S2MPB06_ENABLE_TIME_LDO	180
#define S2MPB06_LDO_N_VOLTAGES	(S2MPB06_LDO_VSEL_MASK + 1)

#endif		/* __S2MPB06_MFD_REGULATOR_H__ */
