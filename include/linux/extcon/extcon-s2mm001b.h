/*
 * extcon-sec.h - Samsung logical extcon drvier to support USB switches
 *
 * This code support for multi MUIC at one device(project).
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 * Author: Seonggyu Park <seongyu.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */


#ifndef __LINUX_EXTCON_S2MM001B_H__
#define __LINUX_EXTCON_S2MM001B_H__

#define S2MM001B_EXTCON_NAME			"s2mm001b"

enum s2mm001b_button_type {
	S2MM001B_BUTTON_SEND,
	S2MM001B_BUTTON_BT1,
	S2MM001B_BUTTON_BT2,
	S2MM001B_BUTTON_BT3,
	S2MM001B_BUTTON_BT4,
	S2MM001B_BUTTON_BT5,
	S2MM001B_BUTTON_BT6,
	S2MM001B_BUTTON_BT7,
	S2MM001B_BUTTON_BT8,
	S2MM001B_BUTTON_BT9,
	S2MM001B_BUTTON_BT10,
	S2MM001B_BUTTON_BT11,
	S2MM001B_BUTTON_BT12,
	S2MM001B_BUTTON_BT_ERR,
	S2MM001B_BUTTON_UNKNOWN,
};

enum s2mm001b_cable_type {		/* RID 	| ADC value	*/
	S2MM001B_CABLE_OTG,		/* 0 	| 00000		*/
	S2MM001B_CABLE_VIDEO,		/* 75	| 00000		*/
	S2MM001B_CABLE_MHL,		/* 1K	| 00000		*/
	S2MM001B_CABLE_PPD,		/* 102K	| 10100		*/
	S2MM001B_CABLE_TTY_CON,		/* 121K	| 10101		*/
	S2MM001B_CABLE_UART,		/* 150K	| 10110		*/
	S2MM001B_CABLE_CARKIT_T1,	/* 200K	| 10111		*/
	S2MM001B_CABLE_JIG_USB_OFF,	/* 255K	| 11000		*/
	S2MM001B_CABLE_JIG_USB_ON,	/* 301K	| 11001		*/
	S2MM001B_CABLE_AV,		/* 365K	| 11010		*/
	S2MM001B_CABLE_CARKIT_T2,	/* 442K	| 11011		*/
	S2MM001B_CABLE_JIG_UART_OFF,	/* 523K	| 11100		*/
	S2MM001B_CABLE_JIG_UART_ON,	/* 619K	| 11101		*/
	S2MM001B_CABLE_USB,		/* open	| 11111		*/
	S2MM001B_CABLE_CDP,		/* open	| 11111		*/
	S2MM001B_CABLE_DCP,		/* open	| 11111		*/
	S2MM001B_CABLE_APPLE_CHG,
	S2MM001B_CABLE_U200,
	S2MM001B_CABLE_UNKNOWN_VB,

	S2MM001B_CABLE_NONE,
};

extern const char *s2mm001b_extcon_cable[CABLE_NAME_MAX + 1];
extern int s2mm001b_set_path(const char *path);
extern struct blocking_notifier_head sec_vbus_notifier;
#endif