/*
 * leds_an30259a_reg.h - panasonic AN30259A led control chip register 
 *                        address map and Register details.
 *
 * Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 */

#ifndef __AN30259A_REG_H__
 #define __AN30259A_REG_H__
 

/* AN30259A register map */
#define AN30259A_REG_SRESET			0x00
#define AN30259A_REG_LEDON			0x01
#define AN30259A_REG_SEL			0x02

#define AN30259A_REG_LED1CC			0x03
#define AN30259A_REG_LED2CC			0x04
#define AN30259A_REG_LED3CC			0x05

#define AN30259A_REG_LED_R			AN30259A_REG_LED1CC
#define AN30259A_REG_LED_G			AN30259A_REG_LED2CC
#define AN30259A_REG_LED_B			AN30259A_REG_LED3CC

#define AN30259A_REG_LED1SLP		0x06
#define AN30259A_REG_LED2SLP		0x07
#define AN30259A_REG_LED3SLP		0x08

#define AN30259A_REG_LED1CNT1		0x09
#define AN30259A_REG_LED1CNT2		0x0a
#define AN30259A_REG_LED1CNT3		0x0b
#define AN30259A_REG_LED1CNT4		0x0c

#define AN30259A_REG_LED2CNT1		0x0d
#define AN30259A_REG_LED2CNT2		0x0e
#define AN30259A_REG_LED2CNT3		0x0f
#define AN30259A_REG_LED2CNT4		0x10

#define AN30259A_REG_LED3CNT1		0x11
#define AN30259A_REG_LED3CNT2		0x12
#define AN30259A_REG_LED3CNT3		0x13
#define AN30259A_REG_LED3CNT4		0x14
#define AN30259A_REG_MAX			0x15


/* Register MASK */
#define AN30259A_MASK_IMAX		0xc0
#define AN30259A_MASK_CLK_MODE	0x38
#define AN30259A_MASK_DELAY		0xf0
#define AN30259A_SRESET			0x01
#define LED_SLOPE_MODE			0x10
#define ALL_LED_SLOPE_MODE		0x70
#define LED_ON					0x01
#define ALL_LED_ON				0x07

#define DUTYMAX_MAX_VALUE		0x7f
#define DUTYMIN_MIN_VALUE		0x00
#define SLPTT_MAX_VALUE			7500

#define AN30259A_TIME_UNIT		500

#define LED_R_MASK			0x00ff0000
#define LED_G_MASK			0x0000ff00
#define LED_B_MASK			0x000000ff
#define LED_R_SHIFT			16
#define LED_G_SHIFT			8
#define LED_IMAX_SHIFT			6
#define LED_CLK_MODE_SHIFT		3
#define AN30259A_CTN_RW_FLG		0x80

#define AN30259A_CLK_OUT_MODE		0x6
#define AN30259A_CLK_IN_MODE	0x4
#define AN30259A_EXTN_PWM_MODE		0x5

#define LED_MAX_CURRENT		0xFF
#define LED_OFF				0x00

#define	MAX_NUM_LEDS	3


#endif //__AN30259A_REG_H__
