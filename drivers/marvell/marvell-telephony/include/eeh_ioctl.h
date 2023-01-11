/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.

 *(C) Copyright 2007 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef __EEH_IOCTL__
#define __EEH_IOCTL__

/* The retun value of the EEH API finction calls. */
typedef unsigned int EEH_STATUS;

/* Error code */
#define EEH_NO_ERROR		0
#define EEH_SUCCESS		EEH_NO_ERROR
#define EEH_ERROR		(EEH_NO_ERROR - 1)
#define EEH_ERROR_INIT		(EEH_NO_ERROR - 2)
#define EEH_MSG_DESC_LEN	512

#define SEH_IOC_MAGIC 'Y'
#define SEH_IOCTL_API  _IOW(SEH_IOC_MAGIC, 1, int)
#define SEH_IOCTL_TEST _IOW(SEH_IOC_MAGIC, 2, int)
#define SEH_IOCTL_APP_ASSERT            _IOW(SEH_IOC_MAGIC, 4, int)
#define SEH_IOCTL_EMULATE_PANIC			_IOW(SEH_IOC_MAGIC, 13, int)
#define SEH_IOCTL_SET_ERROR_INFO		_IOW(SEH_IOC_MAGIC, 14, int)
#define SEH_IOCTL_CP_SILENT_RESET    _IOW(SEH_IOC_MAGIC, 15, int)
#define SEH_IOCTL_APP_CRASH    _IOW(SEH_IOC_MAGIC, 16, int)
#define SEH_IOCTL_SET_EECONFIG_B_CP_RESET    _IOW(SEH_IOC_MAGIC, 17, int)

typedef enum _EehApiId {
	_EehInit = 0,
	_EehDeInit,
	_EehInsertComm2Reset,
	_EehChangeResetState,
	_EehReleaseCommFromReset,
	_EehGetCPLoadAddr,
} EehApiId;

typedef struct _EehApiParams {
	int eehApiId;
	unsigned int status;
	void *params;
} EehApiParams;

typedef enum _EehMsgInd {
	EEH_INVALID_MSG = 0,
	EEH_WDT_INT_MSG,
	EEH_AP_ASSERT_MSG,
	EEH_CP_SILENT_RESET_MSG,
	EEH_AP_CRASH_MSG,
} EehMsgInd;
typedef enum _EehAssertType {
	EEH_AP_ASSERT = 0,
	EEH_CP_EXCEPTION = 1,
	EEH_AP_EXCEPTION = 2,
	EEH_CP_ASSERT,
	EEH_NONE_ASSERT,
} EehAssertType;

typedef struct _EehInsertComm2ResetParam {
	unsigned int AssertType;
} EehInsertComm2ResetParam;

typedef struct _EehMsgStruct {
	unsigned int msgId;
	char msgDesc[EEH_MSG_DESC_LEN];	/* for save AP assert description */
	char processName[NAME_MAX + 1];	/* for save process name */
	int force; /* indicate whether force to reset cp or need to
				  check the configuration in configure file */
	int reset_cp;
} EehMsgStruct;

typedef struct _EehAppAssertParam {
	char msgDesc[EEH_MSG_DESC_LEN];
	char processName[NAME_MAX + 1];
} EehAppAssertParam;

typedef struct _EehCpSilentResetParam {
	char msgDesc[EEH_MSG_DESC_LEN];
	int force;
} EehCpSilentResetParam;

typedef struct _EehAppCrashParam {
	char processName[NAME_MAX + 1];
	int reset_cp;
} EehAppCrashParam;

/* Communicate the error info to SEH (for RAMDUMP and m.b. more) */
typedef struct _EehErrorInfo {
	unsigned err;
	char *str;
	unsigned char *regs;
} EehErrorInfo;

typedef struct _EehGetCPLoadAddrParam {
	uint32_t arbel_load_addr;
} EehGetCPLoadAddrParam;

typedef enum _EehModemChipType {
	EEH_MODEM_CHIP_TYPE_PXA988,
	EEH_MODEM_CHIP_TYPE_PXA986,
	EEH_MODEM_CHIP_TYPE_PXA1088,
	EEH_MODEM_CHIP_TYPE_PXA1L88,
	EEH_MODEM_CHIP_TYPE_EDEN,
	EEH_MODEM_CHIP_TYPE_UNKNOWN
} EehModemChipType;

/* err: SEH converts these into what defined in kernel ramdump_defs.h file */
#define ERR_EEH_CP 0
#define ERR_EEH_AP 1

#endif
