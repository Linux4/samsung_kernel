/*
 * smb328_charger.h
 * Samsung SMB328 Charger Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SMB328_CHARGER_H
#define __SMB328_CHARGER_H __FILE__

#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_KT) || \
	defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
#define SIOP_CHARGING_CURRENT_LIMIT_FEATURE
#endif

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define SEC_CHARGER_I2C_SLAVEADDR (0x69 >> 1)

#define CMD_A_ALLOW_WRITE	BIT(7)
#define CMD_CHARGE_EN                   BIT(4)
#define CFG_AICL_ENABLE			BIT(2)
#define CFG_OTG_ENABLE			BIT(5)
#define CFG_AUTOMATIC_INPUT_CURRENT_LIMIT BIT(4)
#define CFG_AICL_VOLTAGE	0x03
#define CFG_FLOAT_VOLTAGE_MASK		0x7E
#define CFG_FLOAT_VOLTAGE_SHIFT		1
#define CFG_CHARGE_CURRENT_MASK		0xE0
#define CFG_CHARGE_CURRENT_SHIFT	5
#define CFG_INPUT_CURRENT_MASK          0xE0
#define CFG_INPUT_CURRENT_SHIFT         5
#define CFG_TERMINATION_CURRENT_MASK	0x07
#define CFG_TERMINATION_CURRENT_SHIFT	0
#define CFG_STAT_ASSETION_TERM_MASK	0xE0
#define CFG_STAT_ASSETION_TERM_SHIFT	5

/* Register define */
#define SMB328_CHARGE_CURRENT                   0X00
#define SMB328_INPUT_CURRENTLIMIT               0X01
#define SMB328_FLOAT_VOLTAGE                    0X02
#define SMB328_FUNCTION_CONTROL_A               0X03
#define SMB328_FUNCTION_CONTROL_B               0X04
#define SMB328_FUNCTION_CONTROL_C               0X05
#define SMB328_OTG_POWER_AND_LDO                0x06
#define SMB328_VARIOUS_FUNCTIONS                0x07
#define SMB328_OTHER_CONFIGURATION_SELECTION    0x08
#define SMB328_INTERRUPT_SIGNAL_SELECTION       0x09

#define SMB328_COMMAND                          0x31
#define SMB328_INTERRUPT_STATUS_A               0x32
#define SMB328_BATTERY_CHARGING_STATUS_A        0x33
#define SMB328_INTERRUPT_STATUS_B               0x34
#define SMB328_BATTERY_CHARGING_STATUS_B        0x35
#define SMB328_BATTERY_CHARGING_STATUS_C        0x36
#define SMB328_INTERRUPT_STATUS_C               0x37
#define SMB328_BATTERY_CHARGING_STATUS_D        0x38
#define SMB328_AUTOMATIC_INPUT_CURRENT_STATUS   0x39

#endif /* __SMB328_CHARGER_H */
