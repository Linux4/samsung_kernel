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
void
twtPlannerCheckTeardownSuspend(
	struct ADAPTER *prAdapter,
	bool fgForce,
	bool fgTeardown)
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

	if(fgTeardown)
		twtPlannerSendReqTeardown(prAdapter,
			prStaRec, prTWTAgrt->ucFlowId);
	else
		twtPlannerSendReqSuspend(prAdapter,
			prStaRec, prTWTAgrt->ucFlowId);
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
	uint16_t u2CmdLen = 0, u2MsgSize = 0;
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT
		rChipConfigInfo = {0};

	u2CmdLen = kalStrLen(pcCommand);

	/* Prepare command msg to FW */
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
		"Resp=%s, Type[%u], MsgLen[%u], BufLen[%u]\n",
		rChipConfigInfo.aucCmd, rChipConfigInfo.ucRespType,
		rChipConfigInfo.u2MsgSize, u4BufLen);

	if (u2MsgSize > sizeof(rChipConfigInfo.aucCmd)) {
		DBGLOG(REQ, INFO, "invalid ret u2MsgSize\n");
		return -1;
	}

	if(u2MsgSize > 0)
		snprintf(rsp, u2MsgSize, "%s",
		rChipConfigInfo.aucCmd);

	return u2MsgSize;
}

int32_t twt_check_channelusage(
	struct wiphy *wiphy,
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
	bool isImmediately)
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
				prTWTParamSetMsg->rTWTCtrl.ucBssIdx = ucBssIndex;
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
		i4BytesWritten = -1;
	}

	return i4BytesWritten;
}

int twt_teardown_all(
	struct wiphy *wiphy, struct ADAPTER *prAdapter)
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
				prAdapter, ai4Setting[i], FALSE);
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
	uint32_t u4tempout = 0;
	uint64_t u8NextTWT = 0;
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
		i4BytesWritten = -1;
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

		i4BytesWritten = twt_check_channelusage(wiphy, prAdapter);

		if (i4BytesWritten < 0) {
			kalSprintf(rsp, "%d", i4BytesWritten);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
							rsp, sizeof(rsp));
		}

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
		if (!prBssInfo) {
			DBGLOG(REQ, WARN, "ucBssIndex:%d not found\n");
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
			i4BytesWritten = twt_teardown(wiphy, prAdapter, ai4Setting[0], TRUE);
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
				if (ai4Setting[4] >= 256)
					tempout = ai4Setting[4]/256;
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
					tempout = ai4Setting[9]/256;
				break;
			case TWT_PARAMS_WAKEDURMAX:
				if(ai4Setting[10] >= 0)
					tempout = ai4Setting[10]/256;
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
			i4BytesWritten = twt_teardown(wiphy, prAdapter, ai4Setting[0], FALSE);
		} else if(ai4Setting[0] == 0) {
			i4BytesWritten = twt_teardown_all(wiphy, prAdapter);
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
	}else if ((strnicmp(apcArgv[0], "GET_TWT_ST ATUS", 14) == 0) && (i4Argc == TWT_STATUS_STATISTICS_PARAMS)) {
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
	} else if ((strnicmp(apcArgv[0], "GET_TWT_STATISTICS", 18) == 0) && (i4Argc == TWT_STATUS_STATISTICS_PARAMS)) {
		kalSprintf(pcCommand+18, " %d", ai4Setting[0]);
		twt_get_status(wiphy, prAdapter, pcCommand, rsp);
		return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, sizeof(rsp));
	} else if ((strnicmp(apcArgv[0], "CLEAR_TWT_STATISTICS", 20) == 0) && (i4Argc == TWT_STATUS_STATISTICS_PARAMS)) {
		kalSprintf(pcCommand+20, " %d", ai4Setting[0]);
		twt_get_status(wiphy, prAdapter, pcCommand, rsp);
		return mtk_cfg80211_process_str_cmd_reply(wiphy,
						rsp, sizeof(rsp));
	} else if ((strnicmp(apcArgv[0], "TWT_INFO_FRM", 14) == 0) && (i4Argc == TWT_TEARDOWN_SUSPEND_PARAMS)) {
		if (twt_check_is_setup(wiphy, prAdapter, ai4Setting[0])) {
			i4Ret = kalStrtoul(apcArgv[2], 0, (unsigned long *)&u8NextTWT);
			if(u8NextTWT >= 1000000)
				u8NextTWT = u8NextTWT/1000000;
			else
				u8NextTWT = 1;

			index += kalSprintf(cmd + index, "%s", twtprefix);
			for (i = 0; i < 5; i++) {
				switch (i) {
				case TWT_PARAMS_ACTION:
					u4tempout = TWT_PARAM_ACTION_RESUME;
					break;
				case TWT_PARAMS_FLOWID:
					u4tempout = (uint32_t) ai4Setting[0];
					break;
				case TWT_PARAMS_NEXT_TWT_SIZE:
					u4tempout = 1;
					break;
				case TWT_PARAMS_NEXT_TWT_1:
					u4tempout = (uint32_t) (0x00000000FFFFFFFF & u8NextTWT);
					break;
				case TWT_PARAMS_NEXT_TWT_2:
					u4tempout = (uint32_t)((u8NextTWT & 0xFFFFFFFF00000000)>>32);
					break;
				default:
					break;
				};
				index += kalSprintf(cmd + index, " %d", u4tempout);
			}
			kalMemCopy(cmd + index, separator, 1);
			index++;
			prNetdev = wlanGetNetDev(prGlueInfo, ucBssIndex);
			if (!prNetdev) {
				DBGLOG(REQ, ERROR, "prNetdev error!\n");
				return -1;
			}
			i4BytesWritten = priv_driver_cmds(prNetdev, cmd, index);

		} else {
			i4BytesWritten = -1;
		}
	}else {
		DBGLOG(REQ, WARN, "twtparams wrong argc : %d\n", i4Argc);
		i4BytesWritten = -1;
	}

	kalSprintf(rsp, "%d", i4BytesWritten);
	return mtk_cfg80211_process_str_cmd_reply(wiphy,
					rsp, sizeof(rsp));
}

int scheduledpm_set_para(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	char prcmd[256] = {0};
	int32_t ai4Setting[7] = {0};
	char scheduledPMprefix[] = "SCHED_PM";
	int32_t i4Argc = 0;
	uint16_t i = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t index = 0;
	int32_t i4BytesWritten = 0;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u2MsgSize = 0;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;

	WIPHY_PRIV(wiphy, prGlueInfo);

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter null");
		return -1;
	}
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if ((i4Argc == 7) ||
		(i4Argc == 1)) {
		for (i = 0; i < (i4Argc - 1); i++)
			kalStrtoint(apcArgv[i + 1], 0, &(ai4Setting[i]));
	} else {
		DBGLOG(REQ, WARN, "scheduled PM wrong argc : %d\n", i4Argc);
		return -EOPNOTSUPP;
	}
	index += kalSprintf(prcmd + index, "%s", scheduledPMprefix);

	if ((strnicmp(apcArgv[0], "SCHED_PM_SETUP", 14) == 0) && (i4Argc == 7)) {
		index += kalSprintf(prcmd + index, " %d", 1);
		for (i = 0; i < (i4Argc - 1); i++){
			kalStrtoint(apcArgv[i + 1], 0, &(ai4Setting[i]));
			if ((i <= 1 && ai4Setting[i] <= 0) ||
				(i > 1 && ai4Setting[i] < -1)) {
				DBGLOG(REQ, WARN, "scheduled PM wrong para : %d\n", ai4Setting[i]);
				return -EOPNOTSUPP;
			}
			if(ai4Setting[i] > 0)
				index += kalSprintf(prcmd + index, " %d", ai4Setting[i]);
		}
	} else if ((strnicmp(apcArgv[0], "SCHED_PM_TEARDOWN", 17) == 0) && (i4Argc == 1)){
		index += kalSprintf(prcmd + index, " %d", 2);
	} else if ((strnicmp(apcArgv[0], "GET_SCHED_PM_STATUS", 19) == 0) && (i4Argc == 1)){
		index += kalSprintf(prcmd + index, " %d", 0);
	} else if ((strnicmp(apcArgv[0], "SCHED_PM_SUSPEND", 16) == 0) && (i4Argc == 1)){
		index += kalSprintf(prcmd + index, " %d", 3);
	} else if ((strnicmp(apcArgv[0], "SCHED_PM_RESUME", 15) == 0) && (i4Argc == 1)){
		index += kalSprintf(prcmd + index, " %d", 4);
	} else{
		DBGLOG(REQ, WARN,
			"scheduled PM wrong argc : %d apcArgv[0]=%s\n", i4Argc, apcArgv[0]);
		return -EOPNOTSUPP;
	}

	DBGLOG(REQ, WARN,
		"prcmd=%s i4Argc=%d apcArgv=%s\n", prcmd, i4Argc, apcArgv[0]);
	kalMemCopy(prcmd + index, "\0", 1);
	rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_ASCII;
	rChipConfigInfo.u2MsgSize = index;
	kalStrnCpy(rChipConfigInfo.aucCmd, prcmd,
		   CHIP_CONFIG_RESP_SIZE - 1);
	rChipConfigInfo.aucCmd[CHIP_CONFIG_RESP_SIZE - 1] = '\0';
	rStatus = kalIoctl(prAdapter->prGlueInfo, wlanoidQueryChipConfig,
		&rChipConfigInfo, sizeof(rChipConfigInfo), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "%s: kalIoctl ret=%d\n", __func__,
			   rStatus);
		return -1;
	}
	rChipConfigInfo.aucCmd[CHIP_CONFIG_RESP_SIZE - 1] = '\0';

	/* Check respType */
	u2MsgSize = rChipConfigInfo.u2MsgSize;
	DBGLOG(REQ, INFO, "%s: RespTyep  %u\n", __func__,
		   rChipConfigInfo.ucRespType);
	DBGLOG(REQ, INFO, "%s: u2MsgSize %u\n", __func__,
		   rChipConfigInfo.u2MsgSize);

	if (u2MsgSize > sizeof(rChipConfigInfo.aucCmd)) {
		DBGLOG(REQ, INFO, "%s: u2MsgSize error ret=%u\n",
			   __func__, rChipConfigInfo.u2MsgSize);
		return -1;
	}
	i4BytesWritten = snprintf(prcmd, u2MsgSize, "%s",
			 rChipConfigInfo.aucCmd);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
					prcmd, i4BytesWritten);
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
}
#endif

