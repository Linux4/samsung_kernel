/*
* linux/sensor/dts201a.h
*
* Partron DTS201A Digital Thermopile Sensor module driver
*
* Copyright (C) 2013 Partron Co., Ltd. - Sensor Lab.
* partron (partron@partron.co.kr)
* 
* Both authors are willing to be considered the contact and update points for
* the driver.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*
*/
/******************************************************************************
 Revision 1.0.0 2013/Nov/22:
	first release

******************************************************************************/

#ifndef	__DTS201A_H__
#define	__DTS201A_H__

#define	DTS_VENDOR		"PARTRON"
#define	DTS_CHIP_ID		"DTS201A"
#define	DTS_DEV_NAME		"dts201a"

#define	DTS_I2C_WRITE	0
#define	DTS_I2C_READ	1
#define	DTS201A_SADDR	(0x00>>1)

#define	DTS201A_ID0 0x0000
#define	DTS201A_ID1 0x0000
//#define	DTS201A_ID0 0x3026
//#define	DTS201A_ID1 0x0010
//#define	DTS201A_ID0 0x3027
//#define	DTS201A_ID1 0x0110
//#define	DTS201A_ID0 0x3218
//#define	DTS201A_ID1 0x1110

#define	DTS_DELAY_DEFAULT		200
#define	DTS_DELAY_MINIMUM		50
#define	DTS_CONVERSION_TIME	50

/* CONTROL REGISTERS */
#define	DTS_CUST_ID0		0x00		/* customer ID Low byte */
#define	DTS_CUST_ID1		0x01		/* customer ID High byte */
#define	DTS_SM			0xA0		/* Sensor Measurement */
#define	DTS_SM_USER		0xA1		/* Sensor Measurement User Config */
#define	DTS_SM_AZSM		0xA2		/* Auto-Zero corrected Sensor Measurement */
#define	DTS_SM_AZSM_USER	0xA3		/* Auto-Zero corrected Sensor Measurement User Config */
#define	DTS_TM			0xA4		/* Temperature Measurement */
#define	DTS_TM_USER		0xA5		/* Temperature Measurement User Config */
#define	DTS_TM_AZTM		0xA6		/* Auto-Zero corrected Temperature Measurement */
#define	DTS_TM_AZTM_USER	0xA7		/* Auto-Zero corrected Temperature Measurement User Config */
#define	DTS_START_NOM		0xA8		/* transition to normal mode */
#define	DTS_START_CM		0xA9		/* transition to command mode */
#define	DTS_MEASURE		0xAA		/* measurement cycle and calculation and storage 1*/
#define	DTS_MEASURE1		0xAB		/* measurement cycle and calculation and storage 1*/
#define	DTS_MEASURE2		0xAC		/* measurement cycle and calculation and storage 2*/
#define	DTS_MEASURE4		0xAD		/* measurement cycle and calculation and storage 4*/
#define	DTS_MEASURE8		0xAE		/* measurement cycle and calculation and storage 8*/
#define	DTS_MEASURE16		0xAF		/* measurement cycle and calculation and storage 16*/

#define	DTS_0_ORD_To_LSBS	0x1A

#define EVENT_TYPE_AMBIENT_TEMP	REL_HWHEEL
#define EVENT_TYPE_OBJECT_TEMP	REL_DIAL
//#define EVENT_TYPE_STATUS_TEMP

struct outputdata {
	u32 therm_avg[8];
	u32 therm;	// object temperature
	u32 temp;	// ambient temperature
	u8 status;
};

#ifdef __KERNEL__
struct dts201a_platform_data {
	int irq;
};
#endif /* __KERNEL__ */

#endif  /* __DTS201A_H__ */
