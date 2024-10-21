/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _FW_LOG_ICS_H_
#define _FW_LOG_ICS_H_

#if ((CFG_SUPPORT_ICS == 1) || (CFG_SUPPORT_PHY_ICS == 1))
#define ICS_LOG_CMD_ON_OFF	0
#define ICS_LOG_CMD_SET_LEVEL	1

enum ENUM_ICS_LOG_FLAG {
	ICS_FLAG_DEBUGLOGGER,
	ICS_FLAG_TXTIMEOUT,
	ICS_FLAG_FSM_CONTROL,
};

enum ENUM_ICS_LOG_LEVEL_T {
	ENUM_ICS_LOG_LEVEL_DISABLE,
	ENUM_ICS_LOG_LEVEL_MAC,
};

typedef void (*ics_fwlog_event_func_cb)(int, int, u_int8_t, int);
extern ssize_t wifi_ics_fwlog_write(char *buf, size_t count);
extern void wifi_ics_event_func_register(ics_fwlog_event_func_cb pfFwlog);

int IcsInit(void);
int IcsDeInit(void);
#if CFG_SUPPORT_ICS_TIMER
void IcsTimerInit(struct ADAPTER *prAdapter);
void IcsLogStartWithTimer(struct ADAPTER *prAdapter);
void IcsLogTimeout(struct ADAPTER *prAdapter, uintptr_t ulParamPtr);
#endif /* CFG_SUPPORT_ICS_TIMER */
#endif /* CFG_SUPPORT_ICS */

#endif /*_FW_LOG_ICS_H_*/
