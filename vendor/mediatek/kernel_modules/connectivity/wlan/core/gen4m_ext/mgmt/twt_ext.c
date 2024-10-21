/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /mgmt/twt_ext.c
 */

/*! \file   "twt_ext.c"
 *  \brief This file includes twt ext support.
 *    Detail description.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

#include "precomp.h"
#include "gl_os.h"
#include "gl_wext_priv.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define TWT_SETUP_PARAMS   14
#define TWT_TEARDOWN_SUSPEND_PARAMS     3
#define TWT_CAPABILITY_PARAMS    1
#define TWT_STATUS_STATISTICS_PARAMS    2
#define TWT_PARAMS_ACTION    0
#define TWT_PARAMS_FLOWID    1
#define TWT_PARAMS_SETUPCMD    2
#define TWT_PARAMS_TRIGGER    3
#define TWT_PARAMS_UNANNOUNCED    4
#define TWT_PARAMS_WAKEINTERVALEXP    5
#define TWT_PARAMS_PROTECTION    6
#define TWT_PARAMS_MINWAKEDURATION    7
#define TWT_PARAMS_WAKEINTERVALMAN    8
#define TWT_PARAMS_DESIREWAKETIME    9
#define TWT_PARAMS_WAKEINTVALMIN    10
#define TWT_PARAMS_WAKEINTVALMAX    11
#define TWT_PARAMS_WAKEDURMIN    12
#define TWT_PARAMS_WAKEDURMAX    13
#define TWT_PARAMS_NEXT_TWT_SIZE 2
#define TWT_PARAMS_NEXT_TWT_1 3
#define TWT_PARAMS_NEXT_TWT_2 4
#define TWT_PARAMS_DURATION_LOWERBOUND    4000
#define TWT_PARAMS_DURATION_UPPERBOUND    63000
#define TWT_PARAMS_INTERAL_LOWERBOUND    8000
#define TWT_PARAMS_INTERAL_UPPERBOUND    1000000
#define TWT_STATUS_RSPLEN    4
#define TWT_QUERY_RSPLEN    4
#define TWT_CMD_SETUP_LEN    14
#define TWT_DURATION_UNIT   256

#define SCHED_PM_SETUP_PARAMS   8
#define SCHED_PM_PARAMS_DURATION_LOWERBOUND    8192
#define SCHED_PM_PARAMS_DURATION_UPPERBOUND    64512
#define SCHED_PM_PARAMS_INTERVAL_LOWERBOUND    16384
#define SCHED_PM_PARAMS_INTERVAL_UPPERBOUND    1024000
/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */


/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#ifdef CFG_SUPPORT_TWT_EXT
#if (CFG_SUPPORT_802_11BE_MLO == 1)
void twtmldCheckTeardown(
	struct ADAPTER *prAdapter,
	uint32_t u4ActiveStaBitmap)
{
	if (!prAdapter)
		return;

	if (u4ActiveStaBitmap == 0 ||
		(u4ActiveStaBitmap & (u4ActiveStaBitmap - 1)) != 0) {

		scheduledpm_action(prAdapter, SCHED_PM_TEARDOWN, NULL, FALSE,
			SCHED_TEARDOWN_BY_MLO);

		if (IS_FEATURE_ENABLED(
			prAdapter->rWifiVar.ucTWTRequester))
			twtPlannerCheckTeardownSuspend(prAdapter,
				TRUE, TRUE, TEARDOWN_BY_MLO);
	}
}

struct STA_RECORD *twtmldGetActiveStaRec(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct MLD_STA_RECORD *prMldStarec;
	struct STA_RECORD *sta_rec = NULL, *prStaRec = NULL;
	int i;

	prStaRec = aisGetTargetStaRec(prAdapter, ucBssIndex);
	if (!prStaRec)
		return prStaRec;

	prMldStarec = mldStarecGetByStarec(prAdapter, prStaRec);
	if (!prMldStarec)
		return sta_rec;

	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		if ((prMldStarec->u4ActiveStaBitmap & BIT(i)) == 0)
			continue;

		sta_rec = cnmGetStaRecByIndex(prAdapter, i);
		if (sta_rec)
			break;
	}
	DBGLOG(REQ, TRACE, "u4ActiveStaBitmap=%d, sta=%d\n", prMldStarec->u4ActiveStaBitmap, i);
	return sta_rec;
}
#endif
void
twtEventNotify(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucTWTFlowId,
	struct _TWT_PARAMS_T *prTWTParams,
	enum TWT_EVENT_TYPE eEventType,
	uint8_t ucStatus,
	uint8_t ucReason,
	uint8_t ucNegoType)
{
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct PARAM_TWT_EVENT_NEGOTIATION *prTWTNegoInfo;
	struct PARAM_TWT_EVENT_TEARDOWN *prTWTTeardownInfo;
	struct PARAM_TWT_EVENT_NOTIFICATION *prTWTNotifiInfo;
	uint32_t size = 0;
	uint32_t u4Idx = 0;
	uint8_t ucStaConnectedCnt = 0;
	uint8_t ucp2pBssActiveCnt = 0;
	struct BSS_INFO *prTmpBssInfo = NULL;

	wiphy = wlanGetWiphy();

	if (!wlanGetNetDev(prAdapter->prGlueInfo, ucBssIndex)) {
		DBGLOG(REQ, ERROR, "wlanGetNetDev Null\n");
		return;
	}

	wdev = wlanGetNetDev(
		prAdapter->prGlueInfo, ucBssIndex)->ieee80211_ptr;

	if (eEventType == ENUM_TWT_EVENT_NEGOTIATION) {

		if (!prTWTParams) {
			DBGLOG(REQ, ERROR, "prTWTParams Null\n");
			return;
		}

		size = sizeof(struct PARAM_TWT_EVENT_NEGOTIATION);
		prTWTNegoInfo = kalMemAlloc(size, VIR_MEM_TYPE);
		if (!prTWTNegoInfo) {
			DBGLOG(REQ, ERROR, "alloc event fail\n");
			return;
		}

		kalMemZero(prTWTNegoInfo, size);
		prTWTNegoInfo->id = GRID_TWT_NEGOTIATION;
		prTWTNegoInfo->len = size - 2;
		prTWTNegoInfo->ucSetupId = ucTWTFlowId;
		prTWTNegoInfo->ucStatus = ucStatus;
		prTWTNegoInfo->ucReason = ucReason;
		prTWTNegoInfo->ucNegoType = ucNegoType;
		prTWTNegoInfo->ucFlowType = prTWTParams->fgUnannounced;
		prTWTNegoInfo->ucTriggered = prTWTParams->fgTrigger;
		prTWTNegoInfo->u8WakeTime = prTWTParams->u8TWT; /* TWT nego sucess only, prTWTResult->u8TWT */
		prTWTNegoInfo->u4Duration = prTWTParams->ucMinWakeDur * TWT_DURATION_UNIT;
		prTWTNegoInfo->u4Interval = prTWTParams->u2WakeIntvalMantiss <<
			prTWTParams->ucWakeIntvalExponent;

		mtk_cfg80211_vendor_event_generic_response(
			wiphy, wdev, size, (uint8_t *)prTWTNegoInfo);
		kalMemFree(prTWTNegoInfo, VIR_MEM_TYPE, size);

	} else if (eEventType == ENUM_TWT_EVENT_TEARDOWN) {

		if (ucReason == TEARDOWN_BY_DEFAULT)
			return;

		size = sizeof(struct PARAM_TWT_EVENT_TEARDOWN);
		prTWTTeardownInfo = kalMemAlloc(size, VIR_MEM_TYPE);
		if (!prTWTTeardownInfo) {
			DBGLOG(REQ, ERROR, "alloc event fail\n");
			return;
		}

		kalMemZero(prTWTTeardownInfo, size);
		prTWTTeardownInfo->id = GRID_TWT_TEARDOWN;
		prTWTTeardownInfo->len = size - 2;
		prTWTTeardownInfo->ucSetupId = ucTWTFlowId;
		prTWTTeardownInfo->ucReason = ucReason;

		mtk_cfg80211_vendor_event_generic_response(
			wiphy, wdev, size, (uint8_t *)prTWTTeardownInfo);
		kalMemFree(prTWTTeardownInfo, VIR_MEM_TYPE, size);

	} else if (eEventType == ENUM_TWT_EVENT_NOTIFICATION) {

		if (ucReason == NOTIFI_READY) {
			if (prAdapter->ucTWTTearDownReason != TEARDOWN_BY_MLCHANNEL &&
				prAdapter->ucTWTTearDownReason != TEARDOWN_BY_MLCONNECT &&
				prAdapter->ucTWTTearDownReason != TEARDOWN_BY_CHSWITCH &&
				prAdapter->ucTWTTearDownReason != TEARDOWN_BY_COEX &&
				prAdapter->ucTWTTearDownReason != TEARDOWN_BY_PSDISABLE &&
				prAdapter->ucTWTTearDownReason != TEARDOWN_BY_OTHERS)
				return;

			for (u4Idx = 0; u4Idx < MAX_BSSID_NUM; u4Idx++) {
				prTmpBssInfo = prAdapter->aprBssInfo[u4Idx];

				if(prTmpBssInfo == NULL)
					continue;

				if (IS_BSS_ACTIVE(prTmpBssInfo)) {

					if (IS_BSS_P2P(prTmpBssInfo))
						ucp2pBssActiveCnt += 1;

					if (IS_BSS_AIS(prTmpBssInfo) &&
						prTmpBssInfo->eConnectionState == MEDIA_STATE_CONNECTED)
						ucStaConnectedCnt += 1;
				}
			}

			if (ucStaConnectedCnt != 1 || ucp2pBssActiveCnt > 0)
				return;
		}

		/* Notify framwrok, clear Teardown Reason */
		prAdapter->ucTWTTearDownReason = TEARDOWN_BY_DEFAULT;

		size = sizeof(struct PARAM_TWT_EVENT_NOTIFICATION);
		prTWTNotifiInfo = kalMemAlloc(size, VIR_MEM_TYPE);
		if (!prTWTNotifiInfo) {
			DBGLOG(REQ, ERROR, "alloc event fail\n");
			return;
		}

		kalMemZero(prTWTNotifiInfo, size);
		prTWTNotifiInfo->id = GRID_TWT_NOTIFICATION;
		prTWTNotifiInfo->len = size - 2;
		prTWTNotifiInfo->ucNotification = ucReason;

		mtk_cfg80211_vendor_event_generic_response(
			wiphy, wdev, size, (uint8_t *)prTWTNotifiInfo);
		kalMemFree(prTWTNotifiInfo, VIR_MEM_TYPE, size);

	} else {
		DBGLOG(REQ, ERROR, "unknown event type\n");
	}
}

void
twtPlannerCheckTeardownSuspend(
	struct ADAPTER *prAdapter,
	bool fgForce,
	bool fgTeardown,
	enum TEARDOWN_REASON eReason)
{
	uint8_t i;
	struct _TWT_PLANNER_T *prTWTPlanner = NULL;
	struct _TWT_AGRT_T *prTWTAgrt = NULL;
	bool fgIsFind = FALSE;
	uint32_t u4Idx = 0;
	struct STA_RECORD *prStaRec = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	bool fgIs2GMcc = false, fgIs2GScc = false;
	bool fgIs5GScc = false, fgIs5GMcc = false;
	uint8_t ucLast2GChNum = 0, ucLast5GChNum = 0;

	if (!prAdapter) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Invalid prAdapter\n");
		return;
	}

	prTWTPlanner = &(prAdapter->rTWTPlanner);
	prTWTAgrt = &(prTWTPlanner->arTWTAgrtTbl[0]);

	if(!fgForce) {
		for (u4Idx = 0; u4Idx < MAX_BSSID_NUM; u4Idx++) {
			prBssInfo = prAdapter->aprBssInfo[u4Idx];

			if (IS_BSS_NOT_ALIVE(prAdapter, prBssInfo))
				continue;

			if (prBssInfo->eBand == BAND_2G4) {
				if (ucLast2GChNum != 0 &&
					ucLast2GChNum != prBssInfo->ucPrimaryChannel)
					fgIs2GMcc = true;
				else if (ucLast2GChNum != 0 &&
					ucLast2GChNum == prBssInfo->ucPrimaryChannel)
					fgIs2GScc = true;
				ucLast2GChNum = prBssInfo->ucPrimaryChannel;
			} else if (prBssInfo->eBand == BAND_5G) {
				if (ucLast5GChNum != 0 &&
					ucLast5GChNum != prBssInfo->ucPrimaryChannel)
					fgIs5GMcc = true;
				else if (ucLast5GChNum != 0 &&
					ucLast5GChNum == prBssInfo->ucPrimaryChannel)
					fgIs5GScc = true;
				ucLast5GChNum = prBssInfo->ucPrimaryChannel;
			}
#if (CFG_SUPPORT_WIFI_6G == 1)
			else if (prBssInfo->eBand == BAND_6G) {
				/* Use the same handler as 5G channel */
				if (ucLast5GChNum != 0 &&
					ucLast5GChNum != prBssInfo->ucPrimaryChannel)
					fgIs5GMcc = true;
				else if (ucLast5GChNum != 0 &&
					ucLast5GChNum == prBssInfo->ucPrimaryChannel)
					fgIs5GScc = true;
				ucLast5GChNum = prBssInfo->ucPrimaryChannel;
			}
#endif
		}
	}

	if ((!fgIs2GScc && !fgIs2GMcc &&
		!fgIs5GScc && !fgIs5GMcc) && !fgForce) {
		DBGLOG(TWT_PLANNER, WARN,
			"single BSS or MCC (diff band) case\n");

		return;
	}

	for (i = 0; i < TWT_AGRT_MAX_NUM; i++, prTWTAgrt++) {
		if (prTWTAgrt->fgValid == TRUE) {
			fgIsFind = TRUE;
			break;
		}
	}

	if (!fgIsFind) {
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prTWTAgrt->ucBssIdx);

	if (!prBssInfo) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Invalid prBssInfo\n");

		return;
	}

	prStaRec = prBssInfo->prStaRecOfAP;

	if (!prStaRec) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Invalid prStaRec\n");

		return;
	}

	DBGLOG(TWT_PLANNER, INFO,
		"condtion force[%d:%d], scc/mcc[%d:%d:%d:%d]\n",
		fgForce, fgTeardown,
		fgIs2GScc, fgIs5GScc,
		fgIs2GMcc, fgIs5GMcc);

	if(fgTeardown) {
		prAdapter->ucTWTTearDownReason = eReason;

		twtPlannerSendReqTeardown(prAdapter,
			prStaRec, prTWTAgrt->ucFlowId);
	}
	else {
		twtPlannerSendReqSuspend(prAdapter,
			prStaRec, prTWTAgrt->ucFlowId);
	}

}

void
twtPlannerCheckResume(
	struct ADAPTER *prAdapter)
{
	uint8_t i;
	struct _TWT_PLANNER_T *prTWTPlanner = NULL;
	struct _TWT_AGRT_T *prTWTAgrt = NULL;
	struct _TWT_GET_TSF_CONTEXT_T *prGetTsfCtxt = NULL;
	bool fgIsFind = FALSE;
	struct STA_RECORD *prStaRec = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	uint8_t ucFlowId_real;

	if (!prAdapter) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Invalid prAdapter\n");
		return;
	}

	prTWTPlanner = &(prAdapter->rTWTPlanner);
	prTWTAgrt = &(prTWTPlanner->arTWTAgrtTbl[0]);

	for (i = 0; i < TWT_AGRT_MAX_NUM; i++, prTWTAgrt++) {
		if (prTWTAgrt->fgValid == TRUE) {
			fgIsFind = TRUE;
			break;
		}
	}

	if (!fgIsFind) {
		DBGLOG(TWT_PLANNER, INFO,
			"TWT flow not find\n");
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prTWTAgrt->ucBssIdx);

	if (!prBssInfo) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Invalid prBssInfo\n");

		return;
	}

	prStaRec = prBssInfo->prStaRecOfAP;

	if (!prStaRec) {
		DBGLOG(TWT_PLANNER, ERROR,
			"Invalid prStaRec\n");

		return;
	}

	if (prAdapter->prGlueInfo->fgIsInSuspendMode) {
			DBGLOG(TWT_PLANNER, ERROR,
			"Only allow resume when not in suspend mode\n");
		return;
	}

	if (prStaRec->aeTWTReqState == TWT_REQ_STATE_SUSPENDED) {
		if (twtPlannerDrvAgrtFind(
			prAdapter, prTWTAgrt->ucBssIdx, prTWTAgrt->ucFlowId,
			&ucFlowId_real) < TWT_AGRT_MAX_NUM) {
			prGetTsfCtxt =
				kalMemAlloc(
					sizeof(struct _TWT_GET_TSF_CONTEXT_T),
					VIR_MEM_TYPE);
			if (prGetTsfCtxt == NULL) {
				DBGLOG(TWT_PLANNER, ERROR,
					"mem alloc failed\n");
				return;
			}

			prGetTsfCtxt->ucReason =
				TWT_GET_TSF_FOR_RESUME_AGRT;
			prGetTsfCtxt->ucBssIdx =
				prTWTAgrt->ucBssIdx;
			prGetTsfCtxt->ucTWTFlowId =
				prTWTAgrt->ucFlowId;
			prGetTsfCtxt->fgIsOid = FALSE;
			prGetTsfCtxt->rNextTWT.ucNextTWTSize = 1;
			prGetTsfCtxt->rNextTWT.u8NextTWT = 0; /*0TU*/

			twtPlannerGetCurrentTSF(
				prAdapter,
				prBssInfo,
				prGetTsfCtxt,
				sizeof(*prGetTsfCtxt));
		}
	}
}

bool twt_check_is_exist(
	struct wiphy *wiphy,
	struct ADAPTER *prAdapter,
	int32_t i4SetupFlowId)
{
	struct _TWT_PLANNER_T *prTWTPlanner =
		&(prAdapter->rTWTPlanner);
	struct _TWT_AGRT_T *prTWTAgrt =
		&(prTWTPlanner->arTWTAgrtTbl[0]);
	bool fgIsFind = FALSE;
	uint8_t i;

	for (i = 0; i < TWT_AGRT_MAX_NUM; i++, prTWTAgrt++) {
		if (prTWTAgrt->fgValid == TRUE) {
			fgIsFind = TRUE;
			break;
		}
	}

	if (fgIsFind == TRUE) {
		DBGLOG(REQ, INFO,
			"TWT flow already existed : %d %d\n",
			i4SetupFlowId,
			prTWTAgrt->ucFlowId);
		if (prTWTAgrt->ucFlowId != i4SetupFlowId)
			return TRUE;
		else
			return FALSE;
	} else {
		return FALSE;
	}
}

int twt_get_status(
	struct wiphy *wiphy,
	struct ADAPTER *prAdapter,
	char *pcCommand,
	char *rsp)
{
	int ret = 0;
	uint16_t u2CmdLen = 0, u2MsgSize = 0;
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT
		rChipConfigInfo = {0};

	u2CmdLen = kalStrLen(pcCommand);

	/* Prepare command msg to FW */
	if (u2CmdLen < CHIP_CONFIG_RESP_SIZE) {
		kalMemZero(rChipConfigInfo.aucCmd,
			CHIP_CONFIG_RESP_SIZE);
		rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_ASCII;
		rChipConfigInfo.u2MsgSize = u2CmdLen;
		kalStrCpy(rChipConfigInfo.aucCmd, pcCommand);
		rStatus = kalIoctl(prAdapter->prGlueInfo,
			wlanoidQueryChipConfig,
			&rChipConfigInfo,
			sizeof(rChipConfigInfo),
			&u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, ERROR, "kalIoctl ret=%d\n", rStatus);
			return -1;
		}

		rChipConfigInfo.aucCmd[CHIP_CONFIG_RESP_SIZE - 1] = '\0';

		/* Check FW response msg */
		u2MsgSize = rChipConfigInfo.u2MsgSize;
		DBGLOG(REQ, INFO,
			"Type[%u], MsgLen[%u], BufLen[%u]\n",
			rChipConfigInfo.ucRespType,
			rChipConfigInfo.u2MsgSize, u4BufLen);

		if (u2MsgSize > sizeof(rChipConfigInfo.aucCmd)) {
			DBGLOG(REQ, INFO, "invalid ret u2MsgSize\n");
			return -1;
		}

		if(u2MsgSize > 0) {
			ret = snprintf(rsp, u2MsgSize, "%s",
			rChipConfigInfo.aucCmd);
			if (ret < 0) {
				DBGLOG(REQ, ERROR, "snprintf failed\n");
				return -1;
			}
		}

		return u2MsgSize;

	} else {
		DBGLOG(REQ, ERROR, "invalid cmd length: %u\n", u2CmdLen);
		return -1;
	}
}

int32_t twt_check_channelusage(
	struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBssInfo;
	uint32_t u4Idx;
	uint8_t ucLast2GChNum = 0, ucLast5GChNum = 0;
	bool fgIs2GMcc = false, fgIs2GScc = false;
	bool fgIs5GScc = false, fgIs5GMcc = false;

	for (u4Idx = 0; u4Idx < MAX_BSSID_NUM; u4Idx++) {
		prBssInfo = prAdapter->aprBssInfo[u4Idx];

		if (IS_BSS_NOT_ALIVE(prAdapter, prBssInfo))
			continue;

		if (prBssInfo->eBand == BAND_2G4) {
			if (ucLast2GChNum != 0 &&
				ucLast2GChNum != prBssInfo->ucPrimaryChannel)
				fgIs2GMcc = true;
			else if (ucLast2GChNum != 0 &&
				ucLast2GChNum == prBssInfo->ucPrimaryChannel)
				fgIs2GScc = true;
			ucLast2GChNum = prBssInfo->ucPrimaryChannel;
		} else if (prBssInfo->eBand == BAND_5G) {
			if (ucLast5GChNum != 0 &&
				ucLast5GChNum != prBssInfo->ucPrimaryChannel)
				fgIs5GMcc = true;
			else if (ucLast5GChNum != 0 &&
				ucLast5GChNum == prBssInfo->ucPrimaryChannel)
				fgIs5GScc = true;
			ucLast5GChNum = prBssInfo->ucPrimaryChannel;
		}
#if (CFG_SUPPORT_WIFI_6G == 1)
		else if (prBssInfo->eBand == BAND_6G) {
			/* Use the same handler as 5G channel */
			if (ucLast5GChNum != 0 &&
				ucLast5GChNum != prBssInfo->ucPrimaryChannel)
				fgIs5GMcc = true;
			else if (ucLast5GChNum != 0 &&
				ucLast5GChNum == prBssInfo->ucPrimaryChannel)
				fgIs5GScc = true;
			ucLast5GChNum = prBssInfo->ucPrimaryChannel;
		}
#endif
	}

	if (fgIs2GScc || fgIs2GMcc || fgIs5GScc || fgIs5GMcc) {
		DBGLOG(REQ, WARN,
			"Only in single BSS or MCC (diff band) case\n");

		return -4;
	} else {
		return 0;
	}
}


bool twt_check_is_setup(
	struct wiphy *wiphy,
	struct ADAPTER *prAdapter,
	int32_t i4SetupFlowId)
{
	int32_t ai4Setting[TWT_SETUP_PARAMS] = {0};
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0;
	uint8_t index = 0;
	int32_t i4Ret = 0;
	uint16_t i;
	char cmd[256] = {0};

	index += kalSprintf(cmd + index,
		"%s", "GET_TWT_STATUS");
	index += kalSprintf(cmd + index,
		" %d", i4SetupFlowId);

	twt_get_status(wiphy, prAdapter, cmd, cmd);
	wlanCfgParseArgument(cmd, &i4Argc, apcArgv);
	for (i = 0; i < TWT_STATUS_RSPLEN; i++) {
		i4Ret = kalStrtoint(apcArgv[i],
			0, &(ai4Setting[i]));

		if (i4Ret)
			DBGLOG(REQ, WARN, "Argv error ret=%d\n", i4Ret);
	}

	if ((ai4Setting[0] == i4SetupFlowId) &&
		(ai4Setting[2] != 0) && (ai4Setting[3] != 0))
		return TRUE;
	else
		return FALSE;
}

int twt_teardown(
	struct wiphy *wiphy,
	struct ADAPTER *prAdapter,
	int32_t i4FlowId,
	bool isImmediately,
	enum TEARDOWN_REASON eReason)
{
	int32_t i4BytesWritten = 0;
	char cmd[256] = {0};
	struct net_device *prNetdev = NULL;
	int32_t tempout = 0;
	uint8_t index = 0;
	uint16_t i;
	char twtprefix[] = "SET_TWT_PARAMS";
	char separator[] = "\0";
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	uint8_t ucBssIndex = 0;
	struct _MSG_TWT_PARAMS_SET_T *prTWTParamSetMsg;
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct STA_RECORD *prStaRec = NULL;
#endif

	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	if (twt_check_is_setup(wiphy, prAdapter, i4FlowId)) {
		index += kalSprintf(cmd + index, "%s", twtprefix);
		for (i = 0; i < 2; i++) {
			switch (i) {
			case TWT_PARAMS_ACTION:
				tempout = TWT_PARAM_ACTION_DEL;
				break;
			case TWT_PARAMS_FLOWID:
				tempout = i4FlowId;
				break;
			default:
				break;
			};
			index += kalSprintf(cmd + index, " %d", tempout);
		}

		kalMemCopy(cmd + index, separator, 1);
		index++;
		prNetdev = wlanGetNetDev(prGlueInfo, ucBssIndex);
		if (!prNetdev) {
			DBGLOG(REQ, ERROR, "prNetdev error!\n");
			return -1;
		}

		if (!isImmediately) {
			/* In the case of directly establishing a new TWT session,
				there is no need to notify the framework */
			prAdapter->ucTWTTearDownReason = eReason;
			i4BytesWritten = priv_driver_cmds(prNetdev, cmd, index);
		} else {
			DBGLOG(REQ, INFO, "Action=%d\n", TWT_PARAM_ACTION_DEL);
			DBGLOG(REQ, INFO, "TWT Flow ID=%d\n", i4FlowId);

			if (i4FlowId >= TWT_MAX_FLOW_NUM) {
				DBGLOG(REQ, INFO, "Invalid TWT Params\n");
				return -1;
			}

			prTWTParamSetMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				sizeof(struct _MSG_TWT_REQFSM_RESUME_T));

			if (prTWTParamSetMsg) {
				prTWTParamSetMsg->rMsgHdr.eMsgId = MID_TWT_PARAMS_SET;

#if (CFG_SUPPORT_802_11BE_MLO == 1)
				prStaRec = twtmldGetActiveStaRec(prAdapter, ucBssIndex);
				if (prStaRec)
					prTWTParamSetMsg->rTWTCtrl.ucBssIdx = prStaRec->ucBssIndex;
				else
					prTWTParamSetMsg->rTWTCtrl.ucBssIdx = ucBssIndex;
#else
				prTWTParamSetMsg->rTWTCtrl.ucBssIdx = ucBssIndex;
#endif
				prTWTParamSetMsg->rTWTCtrl.ucCtrlAction = TWT_PARAM_ACTION_DEL;
				prTWTParamSetMsg->rTWTCtrl.ucTWTFlowId = (uint8_t)i4FlowId;

				mboxSendMsg(prAdapter, MBOX_ID_0,
					(struct MSG_HDR *) prTWTParamSetMsg,
					MSG_SEND_METHOD_UNBUF);
			}
			else
				return -1;
		}

	} else {
		i4BytesWritten = -4;
	}

	return i4BytesWritten;
}

int twt_teardown_all(
	struct wiphy *wiphy, struct ADAPTER *prAdapter,
	enum TEARDOWN_REASON eReason)
{
	int32_t i4BytesWritten = 0;
	char twtquerycmd[] = "QUERY_TWT_FLOW";
	char cmd[256] = {0};
	int32_t i4Argc = 0;
	uint16_t i;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t ai4Setting[TWT_SETUP_PARAMS] = {0};

	twt_get_status(wiphy, prAdapter, twtquerycmd, cmd);

	wlanCfgParseArgument(cmd, &i4Argc, apcArgv);
	for (i = 0; i < i4Argc; i++) {
		kalStrtoint(apcArgv[i], 0, &(ai4Setting[i]));
		if(ai4Setting[i] >= 0)
			i4BytesWritten = twt_teardown(wiphy,
				prAdapter, ai4Setting[i], FALSE, eReason);
	}

	return i4BytesWritten;
}

int twt_get_status_all(
	struct wiphy *wiphy,
	struct ADAPTER *prAdapter)
{
	char twtquerycmd[] = "QUERY_TWT_FLOW";
	char twtstatuscmd[] = "GET_TWT_STATUS";
	char rsp[512] = {0};
	char cmd[256] = {0};
	int32_t i4Argc = 0;
	uint8_t index = 0;
	uint16_t rspindex = 0;
	uint16_t i;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t ai4Setting[TWT_SETUP_PARAMS] = {0};

	twt_get_status(wiphy, prAdapter, twtquerycmd, cmd);

	wlanCfgParseArgument(cmd, &i4Argc, apcArgv);

	for (i = 0; i < i4Argc; i++) {
		kalStrtoint(apcArgv[i], 0, &(ai4Setting[i]));
		index = 0;
		if(ai4Setting[i] >= 0)
		{
			index += kalSprintf(cmd + index,
				"%s", twtstatuscmd);
			index += kalSprintf(cmd + index,
				" %d", ai4Setting[i]);
			rspindex += twt_get_status(wiphy, prAdapter, cmd, rsp + rspindex);
		}
	}

	if(i4Argc == 0)
		rspindex += kalSprintf(rsp+rspindex,
			"%s", "0 0 0 0\n");

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
					rsp, sizeof(rsp));
}

int twt_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	char rsp[256];
	int32_t i4BytesWritten = 0;
	char cmd[256] = {0};
	int32_t ai4Setting[TWT_SETUP_PARAMS] = {0};
	int32_t i4Ret = 0;
	uint16_t i;
	uint32_t u4Capability = 0;
	char twtprefix[] = "SET_TWT_PARAMS";
	char separator[] = "\0";
	uint8_t index = 0;
	int32_t tempout = 0, i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t ucBssIndex = 0;
	struct net_device *prNetdev = NULL;
	struct STA_RECORD *prStaRec = NULL;
	struct SCAN_INFO *prScanInfo = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct MLD_STA_RECORD *prMldStaRec = NULL;
	uint32_t u4ActiveStaBitmap = 0;
#endif

	WIPHY_PRIV(wiphy, prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter null");
		return -1;
	}

	if (!prAdapter) {
		DBGLOG(REQ, WARN, "Adapter is not ready\n");
		return -1;
	}

	DBGLOG(REQ, WARN, "pcCommand=%s\n", pcCommand);

	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if ((i4Argc == TWT_SETUP_PARAMS) ||
		(i4Argc == TWT_TEARDOWN_SUSPEND_PARAMS) ||
		(i4Argc == TWT_CAPABILITY_PARAMS) ||
		(i4Argc == TWT_STATUS_STATISTICS_PARAMS)){
		for (i = 0; i < (i4Argc - 1); i++) {
			i4Ret = kalStrtoint(apcArgv[i + 1],
				0, &(ai4Setting[i]));

			if (i4Ret)
				DBGLOG(REQ, WARN, "Argv error ret=%d\n", i4Ret);
		}
	} else {
		DBGLOG(REQ, WARN, "twtparams wrong argc : %d\n", i4Argc);
		i4BytesWritten = -5;
		kalSprintf(rsp, "%d", i4BytesWritten);
		return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, sizeof(rsp));
	}

	prStaRec = aisGetTargetStaRec(prAdapter, ucBssIndex);

	if (!prStaRec) {
		DBGLOG(REQ, ERROR, "No connect to AP\n");
		if (!((strnicmp(apcArgv[0], "GET_TWT_STATUS", 14) == 0) &&
			(i4Argc == TWT_STATUS_STATISTICS_PARAMS))){
			i4BytesWritten = -4;
			kalSprintf(rsp, "%d", i4BytesWritten);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
							rsp, sizeof(rsp));
		}
	}

	if ((strnicmp(apcArgv[0], "TWT_SETUP", 9) == 0) &&
		(i4Argc == TWT_SETUP_PARAMS)){

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		prMldStaRec = mldStarecGetByStarec(prAdapter, prStaRec);
		if (prMldStaRec) {
			u4ActiveStaBitmap = prMldStaRec->u4ActiveStaBitmap;
			if (u4ActiveStaBitmap == 0 ||
				(u4ActiveStaBitmap & (u4ActiveStaBitmap - 1)) != 0) {
					DBGLOG(REQ, WARN, "Only support one link active\n");
					i4BytesWritten = -3;
					kalSprintf(rsp, "%d", i4BytesWritten);
					return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, sizeof(rsp));
				}
		}
#endif

		if (!HE_IS_MAC_CAP_TWT_RSP(
			prStaRec->ucHeMacCapInfo)) {
			i4BytesWritten = -3;
			kalSprintf(rsp, "%d", i4BytesWritten);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
				rsp, sizeof(rsp));
		}

		prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
		if (!(prScanInfo == NULL)) {
			if (prScanInfo->eCurrentState ==
				SCAN_STATE_SCANNING) {
				i4BytesWritten = -10;
				kalSprintf(rsp, "%d", i4BytesWritten);
				return mtk_cfg80211_process_str_cmd_reply(wiphy,
								rsp, sizeof(rsp));
			}
		}

		i4BytesWritten = twt_check_channelusage(prAdapter);

		if (i4BytesWritten < 0) {
			kalSprintf(rsp, "%d", i4BytesWritten);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
							rsp, sizeof(rsp));
		}

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
		if (!prBssInfo) {
			DBGLOG(REQ, WARN, "ucBssIndex:%d not found\n", ucBssIndex);
			return -1;
		}

		if (prBssInfo->ePwrMode == Param_PowerModeCAM) {
			i4BytesWritten = -4;
			kalSprintf(rsp, "%d", i4BytesWritten);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
							rsp, sizeof(rsp));
		}

		if(ai4Setting[4] >= ai4Setting[5] ||
			ai4Setting[4] < TWT_PARAMS_DURATION_LOWERBOUND ||
			ai4Setting[4] > TWT_PARAMS_DURATION_UPPERBOUND ||
			ai4Setting[5] < TWT_PARAMS_INTERAL_LOWERBOUND ||
			ai4Setting[5] > TWT_PARAMS_INTERAL_UPPERBOUND ||
			twt_check_is_exist(wiphy, prAdapter, ai4Setting[0])) {
			i4BytesWritten = -5;
			kalSprintf(rsp, "%d", i4BytesWritten);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
							rsp, sizeof(rsp));
		}

		if (twt_check_is_setup(wiphy, prAdapter, ai4Setting[0])) {
			i4BytesWritten = twt_teardown(wiphy, prAdapter, ai4Setting[0],
			TRUE, TEARDOWN_BY_DEFAULT);
		}

		index += kalSprintf(cmd + index, "%s", twtprefix);
		for (i = 0; i < TWT_CMD_SETUP_LEN; i++) {
			tempout = 0;
			switch (i) {
			case TWT_PARAMS_ACTION:
				if (ai4Setting[1] == 0)
					tempout = TWT_PARAM_ACTION_ADD;
				else if (ai4Setting[1] == 1)
					tempout = TWT_PARAM_ACTION_ADD_BTWT;
				else
					DBGLOG(REQ, WARN,
						"twtparams wrong action : %d\n", ai4Setting[1]);
				break;
			case TWT_PARAMS_FLOWID:
				tempout = ai4Setting[0];
				break;
			case TWT_PARAMS_SETUPCMD:
				tempout = TWT_SETUP_CMD_ID_SUGGEST;
				break;
			case TWT_PARAMS_TRIGGER:
				if (ai4Setting[3] <= 1 && ai4Setting[3] >= 0)
					tempout = ai4Setting[3];
				else
					DBGLOG(REQ, WARN,
						"twtparams wrong trigger enabled : %d\n", ai4Setting[3]);
				break;
			case TWT_PARAMS_UNANNOUNCED:
				if (ai4Setting[2] <= 1 && ai4Setting[2] >= 0)
					tempout = ai4Setting[2];
				else
					DBGLOG(REQ, WARN,
						"twtparams wrong unannounced enabled : %d\n", ai4Setting[2]);
				break;
			case TWT_PARAMS_WAKEINTERVALEXP:
				tempout = 10; /*1024us*/
				break;
			case TWT_PARAMS_PROTECTION:
				tempout = 0; /*disalbe*/
				break;
			case TWT_PARAMS_MINWAKEDURATION:
				if (ai4Setting[4] >= TWT_DURATION_UNIT)
					tempout = ai4Setting[4]/TWT_DURATION_UNIT;
				else
					DBGLOG(REQ, WARN,
						"twtparams wrong wake duration : %d\n", ai4Setting[4]);
				break;
			case TWT_PARAMS_WAKEINTERVALMAN:
				if (ai4Setting[5] >= 1024)
					tempout = ai4Setting[5]/1024;
				else
					DBGLOG(REQ, WARN,
						"twtparams wrong wake interval : %d\n", ai4Setting[5]);
				break;
			case TWT_PARAMS_DESIREWAKETIME:
				if (ai4Setting[6] >= 0)
					tempout = ai4Setting[6];
				else
					DBGLOG(REQ, WARN,
						"twtparams wrong desired wakeup time : %d\n", ai4Setting[6]);
				break;
			case TWT_PARAMS_WAKEINTVALMIN:
				if(ai4Setting[7] >= 0)
					tempout = ai4Setting[7];
				break;
			case TWT_PARAMS_WAKEINTVALMAX:
				if(ai4Setting[8] >= 0)
					tempout = ai4Setting[8];
				break;
			case TWT_PARAMS_WAKEDURMIN:
				if(ai4Setting[9] >= 0)
					tempout = ai4Setting[9]/TWT_DURATION_UNIT;
				break;
			case TWT_PARAMS_WAKEDURMAX:
				if(ai4Setting[10] >= 0)
					tempout = ai4Setting[10]/TWT_DURATION_UNIT;
				break;
			default:
				break;
			};
			index += kalSprintf(cmd + index, " %d", tempout);
		}

		kalMemCopy(cmd + index, separator, 1);
		index++;
		prNetdev = wlanGetNetDev(prGlueInfo, ucBssIndex);
		if (!prNetdev) {
			DBGLOG(REQ, ERROR, "prNetdev error!\n");
			return -1;
		}

		i4BytesWritten = priv_driver_cmds(prNetdev, cmd, index);

	}else if ((strnicmp(apcArgv[0], "TWT_TEARDOWN", 12) == 0) && (i4Argc == TWT_TEARDOWN_SUSPEND_PARAMS)) {
		if (ai4Setting[0] > 0) {
			i4BytesWritten = twt_teardown(wiphy, prAdapter, ai4Setting[0], FALSE, TEARDOWN_BY_HOST);
		} else if(ai4Setting[0] == 0) {
			i4BytesWritten = twt_teardown_all(wiphy, prAdapter, TEARDOWN_BY_HOST);
		}
		else {
			i4BytesWritten = -1;
		}
	} else if ((strnicmp(apcArgv[0], "GET_TWT_CAP", 11) == 0) && (i4Argc == TWT_CAPABILITY_PARAMS)) {

		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucTWTRequester))
			u4Capability |= 0x00010000;
#if (CFG_SUPPORT_TWT_HOTSPOT == 1)
		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucTWTHotSpotSupport))
			u4Capability |= 0x00020000;
#endif
#ifdef CFG_SUPPORT_BTWT
#if (CFG_SUPPORT_BTWT == 1)
		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucBTWTSupport))
			u4Capability |= 0x00040000;
#endif
#endif
		if (HE_IS_MAC_CAP_TWT_RSP(prStaRec->ucHeMacCapInfo))
			u4Capability |= 0x00000002;
		if (HE_IS_MAC_CAP_BTWT_SUPT(prStaRec->ucHeMacCapInfo))
			u4Capability |= 0x00000004;
		if (HE_IS_MAC_CAP_FLEXIBLE_TWT_SHDL(prStaRec->ucHeMacCapInfo))
			u4Capability |= 0x00000008;

		kalSprintf(rsp, "0x%08x", u4Capability);
		return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, sizeof(rsp));
	}else if ((strnicmp(apcArgv[0], "GET_TWT_STATUS", 14) == 0) && (i4Argc == TWT_STATUS_STATISTICS_PARAMS)) {
		if (ai4Setting[0] > 0) {
			kalSprintf(pcCommand+14, " %d", ai4Setting[0]);
			twt_get_status(wiphy, prAdapter, pcCommand, rsp);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
							rsp, sizeof(rsp));
		} else if(ai4Setting[0] == 0) {
			return twt_get_status_all(wiphy, prAdapter);
		} else {
			i4BytesWritten = -1;
		}
	}else {
		DBGLOG(REQ, WARN, "twtparams wrong argc : %d\n", i4Argc);
		i4BytesWritten = -1;
	}

	if (i4BytesWritten > 0)
		i4BytesWritten = 0;

	kalSprintf(rsp, "%d", i4BytesWritten);
	return mtk_cfg80211_process_str_cmd_reply(wiphy,
					rsp, sizeof(rsp));
}

int scheduledpm_set_status(
	struct ADAPTER *prAdapter,
	char *pcCommand)
{
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT
		rChipConfigInfo = {0};
	uint16_t u2CmdLen = 0;
	uint32_t u4BufLen = 0;

	u2CmdLen = kalStrLen(pcCommand);
	DBGLOG(REQ, INFO, "Notify FW %s, strlen=%d\n", pcCommand, u2CmdLen);

	rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_ASCII;
	rChipConfigInfo.u2MsgSize = u2CmdLen;
	kalStrnCpy(rChipConfigInfo.aucCmd, pcCommand, u2CmdLen);
	rStatus = wlanSetChipConfig(prAdapter, &rChipConfigInfo,
			sizeof(rChipConfigInfo), &u4BufLen, FALSE);

	return rStatus;
}

void LeakyApEvent(
	struct ADAPTER *prAdapter,
	uint8_t ucDectionType)
{
	struct SCHED_PM_PARAMS *prSchedPmParams = NULL;

	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter null");
		return;
	}

	prSchedPmParams = &prAdapter->rSchedPmParams;
	if (prSchedPmParams == NULL)
		return;

	DBGLOG(REQ, INFO, "Leaky AP tection: %d, Bss: %d\n",
		ucDectionType, prSchedPmParams->ucBssIdx);

	scheduledpmEventNotify(prAdapter, prSchedPmParams->ucBssIdx,
		ENUM_SCHED_EVENT_LEAKYAP, 0);
}

void
scheduledpmEventNotify(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	enum SCHED_EVENT_TYPE eEventType,
	uint8_t ucReason)
{
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct PARAM_SCHED_EVENT_TEARDOWN *prSchedTeardownInfo;
	struct PARAM_SCHED_EVENT_LEAKYAP *prSchedLeakyInfo;
	uint32_t size = 0;

	wiphy = wlanGetWiphy();

	if (!wlanGetNetDev(prAdapter->prGlueInfo, ucBssIndex)) {
		DBGLOG(REQ, ERROR, "wlanGetNetDev Null\n");
		return;
	}

	wdev = wlanGetNetDev(
		prAdapter->prGlueInfo, ucBssIndex)->ieee80211_ptr;

	 if (eEventType == ENUM_SCHED_EVENT_TEARDOWN) {

		size = sizeof(struct PARAM_SCHED_EVENT_TEARDOWN);
		prSchedTeardownInfo = kalMemAlloc(size, VIR_MEM_TYPE);
		if (!prSchedTeardownInfo) {
			DBGLOG(REQ, ERROR, "alloc event fail\n");
			return;
		}

		kalMemZero(prSchedTeardownInfo, size);
		prSchedTeardownInfo->id = GRID_SCHED_TEARDOWN;
		prSchedTeardownInfo->len = size - 2;
		prSchedTeardownInfo->ucReason = ucReason;

		mtk_cfg80211_vendor_event_generic_response(
			wiphy, wdev, size, (uint8_t *)prSchedTeardownInfo);
		kalMemFree(prSchedTeardownInfo, VIR_MEM_TYPE, size);

	} else if (eEventType == ENUM_SCHED_EVENT_LEAKYAP) {

		size = sizeof(struct PARAM_SCHED_EVENT_LEAKYAP);
		prSchedLeakyInfo = kalMemAlloc(size, VIR_MEM_TYPE);
		if (!prSchedLeakyInfo) {
			DBGLOG(REQ, ERROR, "alloc event fail\n");
			return;
		}

		kalMemZero(prSchedLeakyInfo, size);
		prSchedLeakyInfo->id = GRID_SCHED_LEAKYAP;
		prSchedLeakyInfo->len = size - 2;

		mtk_cfg80211_vendor_event_generic_response(
			wiphy, wdev, size, (uint8_t *)prSchedLeakyInfo);
		kalMemFree(prSchedLeakyInfo, VIR_MEM_TYPE, size);

	} else {
		DBGLOG(REQ, ERROR, "unknown event type\n");
	}
}

int scheduledpm_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t ai4Setting[SCHED_PM_SETUP_PARAMS] = {0};
	int32_t i4Argc = 0;
	uint16_t i = 0;
	uint8_t ucBssIndex = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct STA_RECORD *prStaRec = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	struct SCAN_INFO *prScanInfo = NULL;
	struct SCHED_PM_PARAMS *prSchedPmParams = NULL;
	char rsp[256];
	bool fgIsSetup = FALSE;
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct MLD_STA_RECORD *prMldStaRec = NULL;
	uint32_t u4ActiveStaBitmap = 0;
#endif

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(REQ, ERROR, "prGlueInfo null");
		return -1;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter null");
		return -1;
	}
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if ((i4Argc == SCHED_PM_SETUP_PARAMS) ||
		(i4Argc == 2)) {
		for (i = 0; i < (i4Argc - 1); i++)
			kalStrtoint(apcArgv[i + 1], 0, &(ai4Setting[i]));
	}

	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	prStaRec = aisGetTargetStaRec(prAdapter, ucBssIndex);

	if (!prStaRec) {
		DBGLOG(REQ, ERROR, "No connect to AP\n");
		i4BytesWritten = -4;
		kalSprintf(rsp, "%d", i4BytesWritten);
		return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, sizeof(rsp));
	}

	if ((strnicmp(apcArgv[0], "SCHED_PM_SETUP", 14) == 0) && (i4Argc == SCHED_PM_SETUP_PARAMS)) {

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		prMldStaRec = mldStarecGetByStarec(prAdapter, prStaRec);
		if (prMldStaRec) {
			u4ActiveStaBitmap = prMldStaRec->u4ActiveStaBitmap;
			if (u4ActiveStaBitmap == 0 ||
				(u4ActiveStaBitmap & (u4ActiveStaBitmap - 1)) != 0) {
					DBGLOG(REQ, WARN, "Only support one link active\n");
					i4BytesWritten = -3;
					kalSprintf(rsp, "%d", i4BytesWritten);
					return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, sizeof(rsp));
				}
		}
#endif

		prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
		if (prScanInfo != NULL) {
			if (prScanInfo->eCurrentState ==
				SCAN_STATE_SCANNING) {
				i4BytesWritten = -10;
				kalSprintf(rsp, "%d", i4BytesWritten);
				return mtk_cfg80211_process_str_cmd_reply(wiphy,
								rsp, sizeof(rsp));
			}
		}

		i4BytesWritten = twt_check_channelusage(prAdapter);

		if (i4BytesWritten < 0) {
			kalSprintf(rsp, "%d", i4BytesWritten);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
							rsp, sizeof(rsp));
		}

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
		if (!prBssInfo) {
			DBGLOG(REQ, WARN, "ucBssIndex:%d not found\n", ucBssIndex);
			return -1;
		}

		if (prBssInfo->ePwrMode == Param_PowerModeCAM) {
			i4BytesWritten = -4;
			kalSprintf(rsp, "%d", i4BytesWritten);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
							rsp, sizeof(rsp));
		}

		for (i = 0; i < (i4Argc - 1); i++){
			kalStrtoint(apcArgv[i + 1], 0, &(ai4Setting[i]));
			if ((i <= 1 && ai4Setting[i] <= 0) ||
				(i > 1 && ai4Setting[i] < -1)) {
				DBGLOG(REQ, WARN, "scheduled PM wrong para : %d\n", ai4Setting[i]);
				return -EOPNOTSUPP;
			}
		}

		if(ai4Setting[0] >= ai4Setting[1] ||
			ai4Setting[0] < SCHED_PM_PARAMS_DURATION_LOWERBOUND ||
			ai4Setting[0] > SCHED_PM_PARAMS_DURATION_UPPERBOUND ||
			ai4Setting[1] < SCHED_PM_PARAMS_INTERVAL_LOWERBOUND ||
			ai4Setting[1] > SCHED_PM_PARAMS_INTERVAL_UPPERBOUND) {
			i4BytesWritten = -5;
			kalSprintf(rsp, "%d", i4BytesWritten);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
							rsp, sizeof(rsp));
		}

		prSchedPmParams = &prAdapter->rSchedPmParams;
		if (!prSchedPmParams)
			return -1;

		if (prSchedPmParams->u4Duration > 0
			&& prSchedPmParams->u4Interval > 0)
			fgIsSetup = TRUE;

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		prStaRec = twtmldGetActiveStaRec(prAdapter, ucBssIndex);
		if (prStaRec)
			prSchedPmParams->ucBssIdx = prStaRec->ucBssIndex;
		else
			prSchedPmParams->ucBssIdx = ucBssIndex;
#else
		prSchedPmParams->ucBssIdx = ucBssIndex;
#endif
		prSchedPmParams->u4Duration = (uint32_t)ai4Setting[0];
		prSchedPmParams->u4Interval = (uint32_t)ai4Setting[1];
		prSchedPmParams->u4DurationAdd = (uint32_t)ai4Setting[2];
		prSchedPmParams->u4IntervalMin = (uint32_t)ai4Setting[3];
		prSchedPmParams->u4IntervalMax = (uint32_t)ai4Setting[4];
		prSchedPmParams->u4DurationMin = (uint32_t)ai4Setting[5];
		prSchedPmParams->u4DurationMax = (uint32_t)ai4Setting[6];

		i4BytesWritten = scheduledpm_action(prAdapter, SCHED_PM_SETUP, NULL, fgIsSetup, 0);

	} else if ((strnicmp(apcArgv[0], "SCHED_PM_TEARDOWN", 17) == 0) && (i4Argc == 1)) {
		i4BytesWritten = scheduledpm_action(prAdapter, SCHED_PM_TEARDOWN, NULL, FALSE,
							SCHED_TEARDOWN_BY_DEFAULT);
	} else if ((strnicmp(apcArgv[0], "GET_SCHED_PM_STATUS", 19) == 0) && (i4Argc == 1)) {
		scheduledpm_action(prAdapter, SCHED_PM_GET_STATUS, rsp, FALSE, 0);
		return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, sizeof(rsp));
	} else if ((strnicmp(apcArgv[0], "SCHED_PM_SUSPEND", 16) == 0) && (i4Argc == 1)) {
		i4BytesWritten = scheduledpm_action(prAdapter, SCHED_PM_SUSPEND, NULL, FALSE, 0);
	} else if ((strnicmp(apcArgv[0], "SCHED_PM_RESUME", 15) == 0) && (i4Argc == 1)) {
		i4BytesWritten = scheduledpm_action(prAdapter, SCHED_PM_RESUME, NULL, FALSE, 0);
	} else if ((strnicmp(apcArgv[0], "LEAKY_AP_ACTIVE_DETECTION", 15) == 0) && (i4Argc == 4)) {
		kalStrtoint(apcArgv[3], 0, &(ai4Setting[0]));
		i4BytesWritten = leaky_AP_detect_action(prAdapter, LEAKY_AP_DETECT_ACTIVE,
			(char *)apcArgv[1], (char *)apcArgv[2], (uint32_t)ai4Setting[0], 0);
	} else if ((strnicmp(apcArgv[0], "LEAKY_AP_PASSIVE_DETECTION_START", 15) == 0) && (i4Argc == 2)) {
		i4BytesWritten = leaky_AP_detect_action(prAdapter, LEAKY_AP_DETECT_PASSIVE_START,
			NULL, NULL, (uint32_t)ai4Setting[0], 0);
	} else if ((strnicmp(apcArgv[0], "LEAKY_AP_PASSIVE_DETECTION_END", 15) == 0) && (i4Argc == 1)) {
		i4BytesWritten = leaky_AP_detect_action(prAdapter, LEAKY_AP_DETECT_PASSIVE_END,
			NULL, NULL, 0, 0);
	} else if ((strnicmp(apcArgv[0], "SET_GRACE_PERIOD", 15) == 0) && (i4Argc == 2)) {
		i4BytesWritten = leaky_AP_detect_action(prAdapter, LEAKY_AP_DETECT_GRACE_PERIOD,
			NULL, NULL, 0, (uint32_t)ai4Setting[0]);
	} else {
		DBGLOG(REQ, WARN,
			"scheduled PM wrong argc : %d apcArgv[0]=%s\n", i4Argc, apcArgv[0]);
		return -EOPNOTSUPP;
	}

	kalSprintf(rsp, "%d", i4BytesWritten);
	return mtk_cfg80211_process_str_cmd_reply(wiphy,
					rsp, sizeof(rsp));
}

int scheduledpm_action(
	struct ADAPTER *prAdapter,
	enum SCHED_PM_ACTION_TYPE eType,
	char *rsp,
	bool fgDoubleSetup,
	enum SCHED_TEARDOWN_REASON eReason)
{
	struct SCHED_PM_PARAMS *prSchedPmParams = NULL;
	int32_t i4BytesWritten = 0;
	char prcmd[256] = {0};
	char prrep[256] = {0};
	char scheduledPMprefix[] = "SCHED_PM";
	uint32_t index = 0;

	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter null\n");
		return -1;
	}

	prSchedPmParams = &prAdapter->rSchedPmParams;
	if (prSchedPmParams == NULL)
		return -1;

	index += kalSprintf(prcmd + index, "%s", scheduledPMprefix);

	if (eType == SCHED_PM_SETUP) {
		if (fgDoubleSetup) {
			kalSprintf(prcmd + index, " %d\0", (uint32_t)SCHED_PM_TEARDOWN);
			twt_get_status(NULL, prAdapter, prcmd, prrep);
		}

		index += kalSprintf(prcmd + index, " %d", (uint32_t)eType);
		index += kalSprintf(prcmd + index, " %d", prSchedPmParams->u4Duration);
		index += kalSprintf(prcmd + index, " %d", prSchedPmParams->u4Interval);
		index += kalSprintf(prcmd + index, " %d", prSchedPmParams->u4DurationAdd);
		index += kalSprintf(prcmd + index, " %d", prSchedPmParams->u4IntervalMin);
		index += kalSprintf(prcmd + index, " %d", prSchedPmParams->u4IntervalMax);
		index += kalSprintf(prcmd + index, " %d", prSchedPmParams->u4DurationMin);
		index += kalSprintf(prcmd + index, " %d", prSchedPmParams->u4DurationMax);
		kalMemCopy(prcmd + index, "\0", 1);

		twt_get_status(NULL, prAdapter, prcmd, prrep);

	} else if (eType == SCHED_PM_GET_STATUS) {

		if (rsp == NULL)
			return -1; // need reply buffer

		index += kalSprintf(prcmd + index, " %d", (uint32_t)eType);
		kalMemCopy(prcmd + index, "\0", 1);
		twt_get_status(NULL, prAdapter, prcmd, rsp);

	} else if (eType == SCHED_PM_SUSPEND || eType == SCHED_PM_RESUME
				|| eType == SCHED_PM_TEARDOWN) {
		if (prSchedPmParams->u4Duration > 0
			&& prSchedPmParams->u4Interval > 0) {

			if (eType == SCHED_PM_TEARDOWN &&
				eReason == SCHED_TEARDOWN_BY_MLCHANNEL) {
				i4BytesWritten = twt_check_channelusage(prAdapter);
				if (i4BytesWritten >= 0)
					return i4BytesWritten;
			}

			index += kalSprintf(prcmd + index, " %d", (uint32_t)eType);
			kalMemCopy(prcmd + index, "\0", 1);
			scheduledpm_set_status(prAdapter, prcmd);

			if (eType == SCHED_PM_TEARDOWN) {
				if (eReason != SCHED_TEARDOWN_BY_DEFAULT) {
					/* NAN and P2P/MHS are classified as MLCHANNEL in scheduled PM. */
					if (eReason == SCHED_TEARDOWN_BY_MLCONNECT)
						eReason = SCHED_TEARDOWN_BY_MLCHANNEL;

					scheduledpmEventNotify(prAdapter,
						prSchedPmParams->ucBssIdx,
						ENUM_SCHED_EVENT_TEARDOWN, eReason);
				}

				prSchedPmParams->ucBssIdx = 0;
				prSchedPmParams->u4Duration = 0;
				prSchedPmParams->u4Interval = 0;
				prSchedPmParams->u4DurationAdd = 0;
				prSchedPmParams->u4IntervalMin = 0;
				prSchedPmParams->u4IntervalMax = 0;
				prSchedPmParams->u4DurationMin = 0;
				prSchedPmParams->u4DurationMax = 0;
			}
		}
		else {
			i4BytesWritten = -1;
		}
	} else {
		i4BytesWritten = -1;
	}
	return i4BytesWritten;
}

int leaky_AP_detect_action(
	struct ADAPTER *prAdapter,
	enum LEAKY_AP_ACTION_TYPE eType,
	char *pcSrcIp,
	char *pcTarIp,
	uint32_t u4DetectDuration,
	uint32_t u4GracePeriod)
{
	char leadkyAPprefix[] = "LEAKY_AP_DETECT";
	int32_t i4BytesWritten = 0;
	char prcmd[256] = {0};
	uint32_t index = 0;

	index += kalSprintf(prcmd + index, "%s", leadkyAPprefix);
	index += kalSprintf(prcmd + index, " %d", (uint32_t)eType);

	if (eType == LEAKY_AP_DETECT_ACTIVE) {
		index += kalSprintf(prcmd + index, " %s", pcSrcIp);
		index += kalSprintf(prcmd + index, " %s", pcTarIp);
		index += kalSprintf(prcmd + index, " %d", u4DetectDuration);
	} else if (eType == LEAKY_AP_DETECT_PASSIVE_START) {
		index += kalSprintf(prcmd + index, " %d", u4DetectDuration);
	} else if (eType == LEAKY_AP_DETECT_PASSIVE_END) {
	} else if (eType == LEAKY_AP_DETECT_GRACE_PERIOD) {
		index += kalSprintf(prcmd + index, " %d", u4GracePeriod);
	} else {
		return -1;
	}

	kalMemCopy(prcmd + index, "\0", 1);
	DBGLOG(REQ, INFO, "Notify FW %s\n", prcmd);
	scheduledpm_set_status(prAdapter, prcmd);

	return i4BytesWritten;
}

void twtWaitRspTimeout(struct ADAPTER *prAdapter,
				   uintptr_t ulParamPtr)
{
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;
	struct STA_RECORD *prStaRec = NULL;
	struct _TWT_FLOW_T *prTWTFlow = NULL;
	struct _TWT_PARAMS_T *prTWTSetup = NULL;
	uint8_t ucTWTFlowId = 0;
	uint8_t i;


	prStaRec = aisGetTargetStaRec(prAdapter, ucBssIndex);

	if (!prStaRec) {
		DBGLOG(REQ, ERROR,
			"Invalid prStaRec\n");
		return;
	}

	for (i = 0; i < TWT_MAX_FLOW_NUM; i++) {
		prTWTFlow = &(prStaRec->arTWTFlow[i]);
		if (prTWTFlow != NULL) {
			prTWTSetup = &(prTWTFlow->rTWTParams);
			if (prTWTSetup->ucMinWakeDur > 0 &&
				prTWTSetup->u2WakeIntvalMantiss > 0 &&
				prTWTSetup->ucWakeIntvalExponent > 0) {
				ucTWTFlowId = i;
				break;
			}
		}
	}

	if (prStaRec->aeTWTReqState == TWT_REQ_STATE_WAIT_RSP) {
		twtEventNotify(prAdapter, ucBssIndex,
			ucTWTFlowId, prTWTSetup,
			ENUM_TWT_EVENT_NEGOTIATION,
			1, SETUP_NORSP, 0);
	}

}

int delay_wakeup_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint16_t u2CmdLen = 0, u2MsgSize = 0;
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo = {0};
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;

	WIPHY_PRIV(wiphy, prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter null");
		return -1;
	}

	u2CmdLen = kalStrLen(pcCommand);
	DBGLOG(REQ, WARN, "pcCommand=%s Len[%u]\n", pcCommand, u2CmdLen);

	/* Prepare command msg to FW */
	if (u2CmdLen < CHIP_CONFIG_RESP_SIZE) {
		kalMemZero(rChipConfigInfo.aucCmd, CHIP_CONFIG_RESP_SIZE);
		rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_ASCII;
		rChipConfigInfo.u2MsgSize = u2CmdLen;
		kalStrCpy(rChipConfigInfo.aucCmd, pcCommand);
		rStatus = kalIoctl(prAdapter->prGlueInfo,
			wlanoidQueryChipConfig,
			&rChipConfigInfo, sizeof(rChipConfigInfo), &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, ERROR, "kalIoctl ret=%d\n", rStatus);
			return -1;
		}

		/* Check FW response msg */
		u2MsgSize = rChipConfigInfo.u2MsgSize;
		DBGLOG(REQ, INFO,
			"Resp=%s, Type[%u], MsgLen[%u], BufLen[%u]\n",
			rChipConfigInfo.aucCmd, rChipConfigInfo.ucRespType,
			rChipConfigInfo.u2MsgSize, u4BufLen);

		if (u2MsgSize > sizeof(rChipConfigInfo.aucCmd)) {
			DBGLOG(REQ, INFO, "invalid ret u2MsgSize\n");
			return -1;
		}

		return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rChipConfigInfo.aucCmd, u4BufLen);
	} else {
		DBGLOG(REQ, ERROR, "invalid cmd length: %u\n", u2CmdLen);
		return -1;
	}

}

void DelayedWakeupEventNotify(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucWakeId,
	uint8_t ucPktCnt,
	uint8_t ucDataLen,
	uint8_t *data)
{
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct PARAM_DELAYED_WAKEUP *prDelayedWakeupInfo;
	uint32_t i;
	uint32_t size = 0;

	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter null");
		return;
	}

	/* prepare vendor event */
	wiphy = wlanGetWiphy();

	if (!wlanGetNetDev(prAdapter->prGlueInfo, ucBssIndex)) {
		DBGLOG(REQ, ERROR, "wlanGetNetDev Null\n");
		return;
	}

	wdev = wlanGetNetDev(
		prAdapter->prGlueInfo, ucBssIndex)->ieee80211_ptr;

	size = sizeof(struct PARAM_DELAYED_WAKEUP);
	prDelayedWakeupInfo = kalMemAlloc(size, VIR_MEM_TYPE);
	if (!prDelayedWakeupInfo) {
		DBGLOG(REQ, ERROR, "alloc event fail\n");
		return;
	}

	kalMemZero(prDelayedWakeupInfo, size);
	prDelayedWakeupInfo->id = GRID_DELAYED_WAKEUP;
	prDelayedWakeupInfo->len = size - 2;
	prDelayedWakeupInfo->ucWakeupId = ucWakeId;
	prDelayedWakeupInfo->ucPktCnt = ucPktCnt;
	prDelayedWakeupInfo->ucDataLen = ucDataLen;

	/* covert hex data to string */
	for (i = 0; i < ucDataLen; i++) {
		kalSprintf((prDelayedWakeupInfo->data) + (i * 2), "%02X", data[i]);
	}
	DBGLOG(REQ, INFO, "[DW]: STR[%s]\n", prDelayedWakeupInfo->data);

	mtk_cfg80211_vendor_event_generic_response(
		wiphy, wdev, size, (uint8_t *)prDelayedWakeupInfo);
	kalMemFree(prDelayedWakeupInfo, VIR_MEM_TYPE, size);
}
#endif /* CFG_SUPPORT_TWT_EXT */

