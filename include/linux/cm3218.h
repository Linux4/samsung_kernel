/* include/linux/cm3218.h
 *
 * Copyright (C) 2011 Capella Microsystems Inc.
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

#ifndef __LINUX_CM3218_H
#define __LINUX_CM3218_H

#define CM3218_I2C_NAME "cm3218"

/* if use CM3218 ad, remove this comment to normal definition code */
#define _CM3218_AD

/* Define Slave Address*/
#if  defined(_CM3218_AD)
#define		CM3218_ALS_cmd	(0x90>>1)
#else
#define		CM3218_ALS_cmd	(0x20>>1)
#endif /* ifdef CM3218_AD */

#define		CM3218_check_INI	(0x19>>1)

#define ALS_CALIBRATED		0x6F17

/*cm3218*/
/*Define ALS Command Code*/
#define		ALS_CMD		0x00
#define		ALS_HW		0x01
#define		ALS_LW	    0x02
#define		ALS_READ    0x04

/*for ALS command*/
#define	CM3218_ALS_SM_1		(0 << 11)
#define CM3218_ALS_SM_2		(1 << 11)
#define CM3218_ALS_SM_HALF	(2 << 11)
#define CM3218_ALS_IT_125ms	(0 << 6)
#define CM3218_ALS_IT_250ms	(1 << 6)
#define CM3218_ALS_IT_500ms	(2 << 6)
#define CM3218_ALS_IT_1000ms	(3 << 6)
#define CM3218_ALS_PERS_1	(0 << 4)
#define CM3218_ALS_PERS_2	(1 << 4)
#define CM3218_ALS_PERS_4	(2 << 4)
#define CM3218_ALS_PERS_8	(3 << 4)
#define CM3218_ALS_RES_1      (1 << 2)
#define CM3218_ALS_INT_EN     (1 << 1)
#define CM3218_ALS_INT_MASK   0xFD
/*enable/disable ALS func, 1:disable , 0: enable */
#define CM3218_ALS_SD		(1 << 0)
#define CM3218_ALS_SD_MASK		0xFE

#define LS_PWR_ON					(1 << 0)
#define PS_PWR_ON					(1 << 1)

struct cm3218_platform_data {
	int intr;
	uint16_t levels[10];
	uint16_t golden_adc;
	int (*power) (int, uint8_t);	/* power to the chip */
	uint8_t als_slave_address;
	uint8_t check_interrupt_add;
	uint16_t is_cmd;
};

#endif
