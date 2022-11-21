/**
 * arch/arm/mach-omap2/include/mach/sec_param.h
 *
 * Copyright (C) 2010-2011, Samsung Electronics, Co., Ltd. All Rights Reserved.
 *  Written by System S/W Group, Open OS S/W R&D Team,
 *  Mobile Communication Division.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/**
 * Project Name : OMAP-Samsung Linux Kernel for Android
 *
 * Project Description :
 *
 * Comments : tabstop = 8, shiftwidth = 8, noexpandtab
 */

/**
 * File Name : sec_param.h
 *
 * File Description :
 *
 * Author : System Platform 2
 * Dept : System S/W Group (Open OS S/W R&D Team)
 * Created : 11/Mar/2011
 * Version : Baby-Raccoon
 */

#ifndef __SEC_PARAM_H__
#define __SEC_PARAM_H__

#include "sec_common.h"

#define PARAM_MAGIC			0x72726624
#define PARAM_VERSION			0x13	/* Rev 1.3 */

#define COMMAND_LINE_SIZE		1024
#define PARAM_STRING_SIZE		1024	/* 1024 Characters */
#define PARAM_VERSION_LENGTH		16

#define COUNT_PARAM_FILE1		7
#define MAX_PARAM			22
#define MAX_STRING_PARAM		5

/* default parameter values. */
/* Lock status. */
#define FLASH_LOCK	0
#define FLASH_UNLOCK	1

#define TERMINAL_SPEED			7
#define LOAD_TESTKERNEL		0
#define LOAD_KERNEL2			0

#define BOOT_DELAY			0	/* no wait */
#define CONFIG_DONT_SHOW_PARTITIONS
#define LCD_LEVEL			0x061	/* lcd level */
#define SWITCH_SEL			1	/* set uart path to modem and usb path to pda */
#define PHONE_DEBUG_ON		0	/* set phone debug mode */
#define LCD_DIM_LEVEL			0
#define LCD_DIM_TIME			6	/* set backlight dimming time */
#define FORCE_PRERECOVERY		0	/* enter pre recovery mode, if set */
#define REBOOT_MODE			REBOOT_MODE_RECOVERY
#define NATION_SEL			0	/* set nation specific configuration */
#define DEBUG_LEVEL			CONFIG_SEC_DEBUG_LEVEL
#define SET_DEFAULT_PARAM		0	/* set default param */
#define BATT_CAPACITY			0	/* set initial battery capacity */
#define FLASH_LOCK_STATUS		FLASH_UNLOCK
#define COMMAND_LINE_BASIC		"cachepolicy=writealloc "\
					"mpcore_wdt.mpcore_margin=359 root=/dev/ram0 rw rootwait " \
					"crash_reboot=yes crash_dump=no"

#define COMMAND_LINE_ROOTFS		COMMAND_LINE_BASIC" "

#define COMMAND_LINE			COMMAND_LINE_ROOTFS" init=init console=ttyAMA2,115200n8"
#define NULL_CONSOLE			" console='null'"
#define INIT				" init=init"
#define PRODUCTION			COMMAND_LINE_ROOTFS INIT NULL_CONSOLE
#define FORCED_REBOOT_MODE		8	/* Forced Reboot Mode */
#define CMDLINE_PROD			"product"
#define VERSION_LINE			"I8315XXIE00"

typedef enum {
	__SERIAL_SPEED,
	__LOAD_TESTKERNEL,
	__BOOT_DELAY,
	__LCD_LEVEL,
	__SWITCH_SEL,
	__PHONE_DEBUG_ON,
	__LCD_DIM_LEVEL,
	__LCD_DIM_TIME,
	__FORCE_PRERECOVERY,
	__REBOOT_MODE,
	__NATION_SEL,
	__DEBUG_LEVEL,
	__SET_DEFAULT_PARAM,
	__BATT_CAPACITY,
	__LOAD_KERNEL2,
	__FLASH_LOCK_STATUS,
	__PARAM_INT_14,	/* Reserved. */
	__VERSION,
	__CMDLINE,
	__DELTA_LOCATION,
	__CMDLINE_MODE,
	__PARAM_STR_4/* Reserved. */
} param_idx;

typedef struct _param_int_t {
	param_idx ident;
	signed int  value;
} param_int_t;


typedef struct _param_str_t {
	param_idx ident;
	signed char value[PARAM_STRING_SIZE];
} param_str_t;

typedef struct _param_union_t {
    param_int_t param_int_file1[COUNT_PARAM_FILE1];
    param_int_t param_int_file2[MAX_PARAM - MAX_STRING_PARAM - COUNT_PARAM_FILE1];
}param_union_t;


typedef struct {
	int param_magic;
	int param_version;

	union _param_int_list{
		param_int_t param_list[MAX_PARAM - MAX_STRING_PARAM];
		param_union_t param_union;
	} param_int_list;

	param_str_t param_str_list[MAX_STRING_PARAM];
} status_t;

typedef struct {
    int param_magic;
    int param_version;
    param_int_t param_int_file1[COUNT_PARAM_FILE1];
}param_file1_t;


typedef struct {
    param_int_t param_int_file2[MAX_PARAM - MAX_STRING_PARAM - COUNT_PARAM_FILE1];
    param_str_t param_str_list[MAX_STRING_PARAM];
}param_file2_t;

extern void (*sec_set_param_value) (int idx, void *value);
extern void (*sec_get_param_value) (int idx, void *value);

#define USB_SEL_MASK			(1 << 0)
#define UART_SEL_MASK			(1 << 1)

#endif /* __SEC_PARAM_H__ */
