/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /include/twt_ext.h
 */

/*! \file   "twt_ext.h"
 *  \brief This file includes twt ext support.
 *    Detail description.
 */


#ifndef _TWT_EXT_H
#define _TWT_EXT_H

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

#ifdef CFG_SUPPORT_TWT_EXT
#define DELAY_WAKEUP_DATA_LEN 86

enum TWT_EVENT_TYPE {
	ENUM_TWT_EVENT_NEGOTIATION = 0,
	ENUM_TWT_EVENT_TEARDOWN,
	ENUM_TWT_EVENT_NOTIFICATION
};

enum SETUP_REASON {
	SETUP_ACCEPT = 0,
	SETUP_REJECT,
	SETUP_NORSP,
	SETUP_CORRECT,
	SETUP_NOTACCEPT,
	SETUP_NOSUPPORT,
	SETUP_OTHERS = 255
};

enum TEARDOWN_REASON {
	TEARDOWN_BY_HOST = 0,
	TEARDOWN_BY_PEER,
	TEARDOWN_BY_MLCHANNEL,
	TEARDOWN_BY_MLCONNECT,
	TEARDOWN_BY_CHSWITCH,
	TEARDOWN_BY_COEX,
	TEARDOWN_NORSP,
	TEARDOWN_BY_PSDISABLE,
	TEARDOWN_BY_MLO,
	TEARDOWN_BY_DEFAULT = 254,
	TEARDOWN_BY_OTHERS = 255
};

enum NOTIFI_REASON {
	NOTIFI_READY = 1,
	NOTIFI_SET,
	NOTIFI_CLEAR
};

enum SCHED_PM_ACTION_TYPE {
	SCHED_PM_GET_STATUS = 0,
	SCHED_PM_SETUP,
	SCHED_PM_TEARDOWN,
	SCHED_PM_SUSPEND,
	SCHED_PM_RESUME
};

enum LEAKY_AP_ACTION_TYPE {
	LEAKY_AP_DETECT_ACTIVE = 0,
	LEAKY_AP_DETECT_PASSIVE_START,
	LEAKY_AP_DETECT_PASSIVE_END,
	LEAKY_AP_DETECT_GRACE_PERIOD
};

enum SCHED_TEARDOWN_REASON {
	SCHED_TEARDOWN_BY_ROAM = 0,
	SCHED_TEARDOWN_BY_CHSWITCH,
	SCHED_TEARDOWN_BY_MLCHANNEL,
	SCHED_TEARDOWN_BY_PSDISABLE,
	SCHED_TEARDOWN_BY_MLO,
	SCHED_TEARDOWN_BY_MLCONNECT = 253,
	SCHED_TEARDOWN_BY_DEFAULT,
	SCHED_TEARDOWN_BY_OTHERS
};

enum SCHED_EVENT_TYPE {
	ENUM_SCHED_EVENT_TEARDOWN = 0,
	ENUM_SCHED_EVENT_LEAKYAP
};

typedef enum _ENUM_DELAY_WAKEUP_EVENT_TYPE_T
{
    DELAY_WAKEUP_EVENT_TYPE_WAKEUP_PKT = 1,
    DELAY_WAKEUP_EVENT_TYPE_BUFFER_FULL,
    DELAY_WAKEUP_EVENT_TYPE_TIMEOUT,
    DELAY_WAKEUP_EVENT_TYPE_OTHER,
    DELAY_WAKEUP_EVENT_TYPE_NUM
} ENUM_DELAY_WAKEUP_EVENT_TYPE_T, *P_ENUM_DELAY_WAKEUP_EVENT_TYPE_T;

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

__KAL_ATTRIB_PACKED_FRONT__
struct PARAM_TWT_EVENT_NEGOTIATION {
	uint8_t id;
	uint8_t len;
	uint8_t ucSetupId;
	uint8_t ucStatus;
	uint8_t ucReason;
	uint8_t ucNegoType;
	uint8_t ucFlowType;
	uint8_t ucTriggered;
	uint64_t u8WakeTime;
	uint32_t u4Duration;
	uint32_t u4Interval;
} __KAL_ATTRIB_PACKED__;

__KAL_ATTRIB_PACKED_FRONT__
struct PARAM_TWT_EVENT_TEARDOWN {
	uint8_t id;
	uint8_t len;
	uint8_t ucSetupId;
	uint8_t ucReason;
} __KAL_ATTRIB_PACKED__;

__KAL_ATTRIB_PACKED_FRONT__
struct PARAM_TWT_EVENT_NOTIFICATION {
	uint8_t id;
	uint8_t len;
	uint8_t ucNotification;
} __KAL_ATTRIB_PACKED__;

__KAL_ATTRIB_PACKED_FRONT__
struct PARAM_SCHED_EVENT_TEARDOWN {
	uint8_t id;
	uint8_t len;
	uint8_t ucReason;
} __KAL_ATTRIB_PACKED__;

__KAL_ATTRIB_PACKED_FRONT__
struct PARAM_SCHED_EVENT_LEAKYAP {
	uint8_t id;
	uint8_t len;
} __KAL_ATTRIB_PACKED__;

__KAL_ATTRIB_PACKED_FRONT__
struct PARAM_DELAYED_WAKEUP {
	uint8_t id;
	uint8_t len;
	uint8_t ucWakeupId;
	uint8_t ucPktCnt;
	uint8_t ucDataLen;
	uint8_t data[DELAY_WAKEUP_DATA_LEN];
} __KAL_ATTRIB_PACKED__;

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

#if (CFG_SUPPORT_802_11BE_MLO == 1)
void twtmldCheckTeardown(
	struct ADAPTER *prAdapter,
	uint32_t u4ActiveStaBitmap);

struct STA_RECORD *twtmldGetActiveStaRec(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);
#endif

void twtWaitRspTimeout(struct ADAPTER *prAdapter,
	uintptr_t ulParamPtr);

void twtEventNotify(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucTWTFlowId,
	struct _TWT_PARAMS_T *prTWTParams,
	enum TWT_EVENT_TYPE eEventType,
	uint8_t ucStatus,
	uint8_t ucReason,
	uint8_t ucNegoType);

int twt_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen);

int twt_teardown_all(
	struct wiphy *wiphy,
	struct ADAPTER *prAdapter,
	enum TEARDOWN_REASON eReason);

void twtPlannerCheckTeardownSuspend(
	struct ADAPTER *prAdapter,
	bool fgForceTeardown,
	bool fgTeardown,
	enum TEARDOWN_REASON eReason);

void twtPlannerCheckResume(
	struct ADAPTER *prAdapter);

void LeakyApEvent(
	struct ADAPTER *prAdapter,
	uint8_t ucDectionType);

void scheduledpmEventNotify(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	enum SCHED_EVENT_TYPE eEventType,
	uint8_t ucReason);

int scheduledpm_action(
	struct ADAPTER *prAdapter,
	enum SCHED_PM_ACTION_TYPE eType,
	char *rsp,
	bool fgDoubleSetup,
	enum SCHED_TEARDOWN_REASON eReason);

int leaky_AP_detect_action(
	struct ADAPTER *prAdapter,
	enum LEAKY_AP_ACTION_TYPE eType,
	char *pcSrcIp,
	char *pcTarIp,
	uint32_t u4DetectDuration,
	uint32_t u4GracePeriod);

int scheduledpm_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen);

int delay_wakeup_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen);

void DelayedWakeupEventNotify(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucWakeId,
	uint8_t ucPktCnt,
	uint8_t ucDataLen,
	uint8_t *data);

#endif /* CFG_SUPPORT_TWT_EXT */
#endif /* _TWT_EXT_H */
