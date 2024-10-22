/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /mgmt/p2p_ext.c
 */

/*! \file   "p2p_ext.c"
 *  \brief This file includes p2p ext support.
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

#if CFG_SUPPORT_QA_TOOL
#include "gl_ate_agent.h"
#include "gl_qa_agent.h"
#endif

#if CFG_SUPPORT_WAPI
#include "gl_sec.h"
#endif
#if CFG_ENABLE_WIFI_DIRECT
#include "gl_p2p_os.h"
#endif

#ifdef CFG_MTK_CONNSYS_DEDICATED_LOG_PATH
#if (CFG_SUPPORT_CONNINFRA == 1)
#include "connsys_debug_utility.h"
#include "metlog.h"
#endif
#endif

#if CFG_SUPPORT_NAN
#include "nan_data_engine.h"
#include "nan_sec.h"
#endif

#if CFG_SUPPORT_CSI
#include "gl_csi.h"
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

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
#if CFG_SAP_RPS_SUPPORT
uint32_t p2pSetSapRps(
	struct ADAPTER *prAdapter,
	u_int8_t fgEnSapRps,
	uint8_t ucSetRpsPhase,
	uint8_t ucBssIdx)
{
	struct CMD_SET_FW_SAP_RPS rForceRps = {0};

	rForceRps.ucForceSapRpsEn = fgEnSapRps;
	rForceRps.ucSapRpsPhase= ucSetRpsPhase;
	rForceRps.ucBssIdx = ucBssIdx;
	DBGLOG(REQ, INFO, "ucForceSapRpsEn = %d\n",
			fgEnSapRps);
	DBGLOG(REQ, INFO, "ucSapRpsPhase = %d\n",
			ucSetRpsPhase);

	wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
		CMD_ID_SET_SAP_RPS, /* ucCID */
		TRUE,	/* fgSetQuery */
		FALSE,	/* fgNeedResp */
		FALSE,	/* fgIsOid */
		NULL,	/* pfCmdDoneHandler */
		NULL,	/* pfCmdTimeoutHandler */
		sizeof(struct CMD_SET_FW_SAP_RPS),
		(uint8_t *)&rForceRps,	/* pucInfoBuffer */
		NULL,	/* pvSetQueryBuffer */
		0	/* u4SetQueryBufferLen */
	);

	return WLAN_STATUS_SUCCESS;
}
#endif

#if CFG_SAP_RPS_SUPPORT
uint32_t p2pSetSapSus(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex, u_int8_t fgEnSapSus)
{
	struct CMD_SET_FW_SAP_SUS rForceSus = {0};

	rForceSus.ucForceSapSusEn = fgEnSapSus;
	rForceSus.ucBssIdx = ucBssIndex;
	DBGLOG(REQ, INFO,
		"ucBssIndex = %d, ucForceSapSusEn = %d\n",
		ucBssIndex, fgEnSapSus);

	wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
				CMD_ID_SET_SAP_SUS, /* ucCID */
				TRUE,	/* fgSetQuery */
				FALSE,	/* fgNeedResp */
				FALSE,	/* fgIsOid */
				NULL,	/* pfCmdDoneHandler */
				NULL,	/* pfCmdTimeoutHandler */
				sizeof(struct CMD_SET_FW_SAP_SUS),
				(uint8_t *)&rForceSus,	/* pucInfoBuffer */
				NULL,	/* pvSetQueryBuffer */
				0	/* u4SetQueryBufferLen */
	);

	return WLAN_STATUS_SUCCESS;
}

u_int8_t p2pFuncIsRpsEnable(struct ADAPTER *prAdapter,
		u_int32_t u32Period,
		signed long * rxPacketDiff,
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo)
{
	u_int8_t i;
	u_int32_t u32TotalIncomePkt = 0;
	for (i = 0; i < MAX_BSSID_NUM; i++) {
		u32TotalIncomePkt = u32TotalIncomePkt + rxPacketDiff[i];
	}

	u32TotalIncomePkt = (u32TotalIncomePkt* u32Period/ 1000);
	DBGLOG(P2P, WARN, "Total Incoming Pkt: %lu\n",
		u32TotalIncomePkt);
	DBGLOG(P2P, WARN, "Incoming Pkt Threshold: %lu\n",
		prAdapter->rWifiVar.u4RpsInpktThresh);
	cnmTimerStopTimer(prAdapter,
			  &(prP2pRoleFsmInfo->rP2pRpsEnterTimer));

	if (u32TotalIncomePkt <=
		prAdapter->rWifiVar.u4RpsInpktThresh)
		prAdapter->rWifiVar.u4RpsMeetTime =
			prAdapter->rWifiVar.u4RpsMeetTime + u32Period;
	else
		prAdapter->rWifiVar.u4RpsMeetTime = 0;
	DBGLOG(P2P, WARN, "RpsMeetTime: %lu\n",
		prAdapter->rWifiVar.u4RpsMeetTime);
	DBGLOG(P2P, WARN, "RpsTriggerTime: %lu\n",
		prAdapter->rWifiVar.u4RpsTriggerTime);

	if (prAdapter->rWifiVar.u4RpsMeetTime >=
		prAdapter->rWifiVar.u4RpsTriggerTime)
		return TRUE;
	else
		return FALSE;
}

void p2pFuncRpsAisCheck(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo,
		uint8_t ucIsEnable)
{
	struct BSS_INFO * prSapBss;
	struct BSS_INFO *bssNext;

	prSapBss = cnmGetSapBssInfo(prAdapter);
	prAdapter->rWifiVar.fgSapRpsAisCond = ucIsEnable;
	DBGLOG(SW4, WARN,
		"ais enable: %u, AisCond: %u\n",
		ucIsEnable,
		prAdapter->rWifiVar.fgSapRpsAisCond);

	if (!prSapBss)
		return;
	else
		bssNext =
			cnmGetOtherSapBssInfo(prAdapter, prSapBss);
	if (IS_BSS_AIS(prBssInfo) &&
		ucIsEnable &&
		prAdapter->rWifiVar.fgSapRpsSwitch == TRUE) {
		prAdapter->rWifiVar.fgSapRpsForceOn = 0;
		prAdapter->rWifiVar.fgSapRpsSwitch = FALSE;
		p2pSetSapRps(prAdapter,
			0,
			prAdapter->rWifiVar.ucSapRpsPhase,
			prSapBss->ucBssIndex);
		if (bssNext)
			p2pSetSapRps(prAdapter,
				0,
				prAdapter->rWifiVar.ucSapRpsPhase,
				prSapBss->ucBssIndex);
	}
}

void p2pFuncRpsKalCheck(struct ADAPTER *prAdapter,
		u_int32_t u32Period,
		signed long * rxPacketDiff)
{
	struct BSS_INFO * prSapBss;
	struct BSS_INFO *bssNext;
	uint32_t u4TimeoutMs = 0;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo = NULL;
	u_int8_t fgIsRpsEnable;

	prSapBss = cnmGetSapBssInfo(prAdapter);
	if (!prSapBss)
		return;

	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(
		prAdapter, prSapBss->u4PrivateData);

	if (prSapBss)
		bssNext =
			cnmGetOtherSapBssInfo(prAdapter, prSapBss);
	fgIsRpsEnable = p2pFuncIsRpsEnable(prAdapter,
		u32Period, rxPacketDiff,
		prP2pRoleFsmInfo);

	if ((prAdapter->rWifiVar.fgSapRpsAisCond == TRUE ||
		!fgIsRpsEnable) &&
		prAdapter->rWifiVar.fgSapRpsSwitch == TRUE) {
		prAdapter->rWifiVar.fgSapRpsSwitch = FALSE;
		p2pSetSapRps(prAdapter, FALSE,
			prAdapter->rWifiVar.ucSapRpsPhase,
			prSapBss->ucBssIndex);
		if (bssNext)
			p2pSetSapRps(prAdapter, FALSE,
				prAdapter->rWifiVar.ucSapRpsPhase,
				bssNext->ucBssIndex);
		DBGLOG(SW4, WARN,
				"start sap rps timer: %u ms\n",
				prAdapter->rWifiVar.u4RpsTriggerTime);

		u4TimeoutMs = prAdapter->rWifiVar.u4RpsTriggerTime;
		cnmTimerStartTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRpsEnterTimer),
			u4TimeoutMs);
	} else if (fgIsRpsEnable &&
		prAdapter->rWifiVar.fgSapRpsAisCond == FALSE &&
		prAdapter->rWifiVar.fgSapRpsSwitch == FALSE) {
		prAdapter->rWifiVar.fgSapRpsSwitch = TRUE;
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRpsEnterTimer));
		p2pSetSapRps(prAdapter, TRUE,
			prAdapter->rWifiVar.ucSapRpsPhase,
			prSapBss->ucBssIndex);
		if (bssNext)
			p2pSetSapRps(prAdapter, TRUE,
				prAdapter->rWifiVar.ucSapRpsPhase,
				bssNext->ucBssIndex);
	}

	DBGLOG(SW4, WARN,
		"sap rps enable: %u, AisCond: %u\n",
		prAdapter->rWifiVar.fgSapRpsSwitch,
		prAdapter->rWifiVar.fgSapRpsAisCond);

}
#endif

#if CFG_EXT_FEATURE
void p2pExtStopAp(
	struct GLUE_INFO *prGlueInfo)
{
	uint8_t country[2] = {0};
	uint32_t u4BufLen, rStatus = WLAN_STATUS_SUCCESS;

	if (!prGlueInfo)
		return;

	/* Restore SET_INDOOR_CHANNEL to default when disable hostapd */
	prGlueInfo->rRegInfo.eRegChannelListMap = REG_CH_MAP_COUNTRY_CODE;
	country[0] = (prGlueInfo->prAdapter->rWifiVar.u2CountryCode
			& 0xff00) >> 8;
	country[1] = prGlueInfo->prAdapter->rWifiVar.u2CountryCode
			& 0x00ff;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetCountryCode,
			country, 2, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(P2P, WARN, "Restore SET_INDOOR_CHANNEL fail!\n");
	else
		DBGLOG(P2P, INFO,
			"Disable hostapd, restore SET_INDOOR_CHANNEL!\n");
}
#endif

