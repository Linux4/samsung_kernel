/*
 * bq24272_charger.h
 * Samsung BQ24272 Charger Header
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



/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
#ifndef __SMB358_CHARGER_H
#define __SMB358_CHARGER_H

 #include <linux/workqueue.h>
#include <linux/wakelock.h>

#define CHARGER_I2C_BUS_ID 3
#define GPIO_SMB358_INT 142

/* Slave address */
#define SMB358_SLAVE_ADDR		0xD4>>1

/* SMB358 Registers. */
#define SMB358_CHARGE_CURRENT		0X00
#define SMB358_INPUT_CURRENTLIMIT	0X01
#define SMB358_VARIOUS_FUNCTIONS	0X02
#define SMB358_FLOAT_VOLTAGE		0X03
#define SMB358_CHARGE_CONTROL		0X04
#define SMB358_STAT_TIMERS_CONTROL	0x05
#define SMB358_PIN_ENABLE_CONTROL	0x06
#define SMB358_THERM_CONTROL_A		0x07
#define SMB358_SYSOK_USB30_SELECTION	0x08
#define SMB358_OTHER_CONTROL_A	0x09
#define SMB358_OTG_TLIM_THERM_CONTROL	0x0A
#define SMB358_LIMIT_CELL_TEMPERATURE_MONITOR	0x0B
#define SMB358_FAULT_INTERRUPT	0x0C
#define SMB358_STATUS_INTERRUPT	0x0D
#define SMB358_I2C_BUS_SLAVE_ADDR	0x0E

#define SMB358_COMMAND_A	0x30
#define SMB358_COMMAND_B	0x31
#define SMB358_COMMAND_C	0x33
#define SMB358_INTERRUPT_STATUS_A	0x35
#define SMB358_INTERRUPT_STATUS_B	0x36
#define SMB358_INTERRUPT_STATUS_C	0x37
#define SMB358_INTERRUPT_STATUS_D	0x38
#define SMB358_INTERRUPT_STATUS_E	0x39
#define SMB358_INTERRUPT_STATUS_F	0x3A
#define SMB358_STATUS_A	0x3B
#define SMB358_STATUS_B	0x3C
#define SMB358_STATUS_C	0x3D
#define SMB358_STATUS_D	0x3E
#define SMB358_STATUS_E	0x3F

/* Charge current register */
#define FAST_CHG_200mA				(0<<5)
#define FAST_CHG_450mA				(1<<5)
#define FAST_CHG_600mA				(2<<5)
#define FAST_CHG_900mA				(3<<5)
#define FAST_CHG_1300mA				(4<<5)
#define FAST_CHG_1500mA				(5<<5)
#define FAST_CHG_1800mA				(6<<5)
#define FAST_CHG_2000mA				(7<<5)
#define FAST_CHG_MASK					0x1F

#define PRE_CHG_150mA					(0<<3)
#define PRE_CHG_250mA					(1<<3)
#define PRE_CHG_350mA					(2<<3)
#define PRE_CHG_450mA					(3<<3)

#define EOC_30mA						0
#define EOC_40mA						1
#define EOC_60mA						2
#define EOC_80mA						3
#define EOC_100mA						4
#define EOC_125mA						5
#define EOC_150mA						6
#define EOC_200mA						7
#define EOC_MASK						0xF8

/* Input current limit register */
#define LIMIT_300mA						(0<<4)
#define LIMIT_500mA						(1<<4)
#define LIMIT_700mA						(2<<4)
#define LIMIT_1000mA					(3<<4)
#define LIMIT_1200mA					(4<<4)
#define LIMIT_1500mA					(5<<4)
#define LIMIT_1800mA					(6<<4)
#define LIMIT_2000mA					(7<<4)
#define NO_LIMIT							(1<<7)

#define CHG_INHIBIT_THR_50mV			(0<<2)
#define CHG_INHIBIT_THR_100mV			(1<<2)
#define CHG_INHIBIT_THR_200mV			(2<<2)
#define CHG_INHIBIT_THR_300mV			(3<<2)

#define CHG_INHIBIT_ENABLE				(1<<1)
#define ADC_PRE_LOAD_ENABLE			(1<<0)

/* Float Voltage */
#define PRE_TO_FAST_CHG_2_5V			(1<<6)
#define VOLTAGE_4_20V					(0x23)
#define VOLTAGE_4_35V					(0x2B)

/* Charger Control Register */
#define AUTO_RECHG_DISABLE			(1<<7)

/* Command register A */
#define ALLOW_VOLATILE_WRITE	(1<<7)
#define CHG_ENABLE				(1<<1)

/* Command register B */
#define USB_1_MODE				0
#define USB_5_MODE				(1<<1)
#define HC_MODE					(1<<0)

/* Status register C */
#define SMB358_CHARGING_ENABLE	(1 << 0)
#define SMB358_CHARGING_STATUS	(1 << 5)
#define SMB358_CHARGER_ERROR	(1 << 6)


#define CLEAR						0


struct smb358_platform_data {
	unsigned int cd;
	struct delayed_work	stat_irq_work;
	struct wake_lock	stat_irq_wakelock;
	struct delayed_work	set_current_limit_work;
	struct wake_lock	set_current_limit_wakelock;
	
};


struct sec_charger_info {
	struct i2c_client		*client;
	struct smb358_platform_data *pdata;
	struct delayed_work isr_work;

	int cable_type;
	bool is_charging;

	/* register programming */
	int reg_addr;
	int reg_data;
};
#endif

