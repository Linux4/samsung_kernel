/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /mgmt/roaming_ext.c
 */

/*! \file   "roaming_ext.c"
 *  \brief This file includes roaming ext support.
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
#if CFG_ENABLE_WIFI_DIRECT
#include "gl_p2p_os.h"
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define FW_CFG_KEY_ROAM_RCPI			"RoamingRCPIValue"

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

struct PARAM_MANAGE_CHANNEL_LIST {
	uint8_t id;
	uint8_t len;
	uint8_t ssid[ELEM_MAX_LEN_SSID + 1];
	uint8_t num;
	uint32_t frequencies[0];
};

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

#define NCHO_BAND_LIST_2G4      BIT(1)
#define NCHO_BAND_LIST_5G       BIT(2)
#if (CFG_SUPPORT_WIFI_6G == 1)
#define NCHO_BAND_LIST_6G       BIT(3)
#endif

#define FW_CFG_KEY_NCHO_ENABLE			"NCHOEnable"
#define FW_CFG_KEY_NCHO_ROAM_RCPI		"RoamingRCPIValue"
#define FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME	"NCHOScnChannelTime"
#define FW_CFG_KEY_NCHO_SCN_HOME_TIME		"NCHOScnHomeTime"
#define FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME	"NCHOScnHomeAwayTime"
#define FW_CFG_KEY_NCHO_SCN_NPROBES		"NCHOScnNumProbs"
#define FW_CFG_KEY_NCHO_WES_MODE		"NCHOWesMode"
#define FW_CFG_KEY_NCHO_SCAN_DFS_MODE		"NCHOScnDfsMode"
#define FW_CFG_KEY_NCHO_SCAN_PERIOD		"NCHOScnPeriod"

#define PERCENTAGE(_val, _base) (_val * 100 / _base)

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if (CFG_EXT_ROAMING == 1)
static uint8_t *apucBandStr[BAND_NUM] = {
	(uint8_t *) DISP_STRING("NULL"),
	(uint8_t *) DISP_STRING("2.4G"),
	(uint8_t *) DISP_STRING("5G"),
#if (CFG_SUPPORT_WIFI_6G == 1)
	(uint8_t *) DISP_STRING("6G")
#endif
};

#if (CFG_SUPPORT_APS == 1)
extern uint8_t apsGetCuInfo(struct ADAPTER *ad,
	struct BSS_DESC *bss,
	uint8_t bidx);
#endif

static uint8_t apsGetCuInfoEx(
	struct ADAPTER *ad,
	struct BSS_DESC *bss,
	uint8_t bidx)
{
#if (CFG_SUPPORT_APS == 1)
	return apsGetCuInfo(ad, bss, bidx);
#else
#if (CFG_EXT_VERSION == 1)
#error "Force to use aps"
#endif
	/* Todo */
	return 0;
#endif
}


static uint32_t apsCalculateRssiFactor(
	struct ADAPTER *ad,
	struct BSS_DESC *bss,
	enum ENUM_ROAMING_REASON eRoamType,
	uint8_t bidx)
{
	uint32_t u4RssiFactor = 0;
	int8_t cRssi = RCPI_TO_dBm(bss->ucRCPI);
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;

	if (bss->eBand == BAND_2G4) {
		if (cRssi >= prWifiVar->cB1RssiFactorVal1)
			u4RssiFactor = prWifiVar->ucB1RssiFactorScore1;
		else if (cRssi < prWifiVar->cB1RssiFactorVal1 &&
				cRssi >= prWifiVar->cB1RssiFactorVal2)
			u4RssiFactor = prWifiVar->ucB1RssiFactorScore2 +
				(2 * (60 + cRssi));
		else if (cRssi < prWifiVar->cB1RssiFactorVal2 &&
				cRssi >= prWifiVar->cB1RssiFactorVal3)
			u4RssiFactor = prWifiVar->ucB1RssiFactorScore3 +
				(3 * (70 + cRssi));
		else if (cRssi < prWifiVar->cB1RssiFactorVal3 &&
				cRssi >= prWifiVar->cB1RssiFactorVal4)
			u4RssiFactor = prWifiVar->ucB1RssiFactorScore4 +
				(4 * (80 + cRssi));
		else if (cRssi < prWifiVar->cB1RssiFactorVal4 &&
				cRssi >= prWifiVar->cB1RssiFactorVal5)
			u4RssiFactor = prWifiVar->ucB1RssiFactorScore5 +
				(2 * (90 + cRssi));
	} else if (bss->eBand == BAND_5G) {
		if (cRssi >= prWifiVar->cB2RssiFactorVal1)
			u4RssiFactor = prWifiVar->ucB2RssiFactorScore1;
		else if (cRssi < prWifiVar->cB2RssiFactorVal1 &&
				cRssi >= prWifiVar->cB2RssiFactorVal2)
			u4RssiFactor = prWifiVar->ucB2RssiFactorScore2 +
				(2 * (60 + cRssi));
		else if (cRssi < prWifiVar->cB2RssiFactorVal2 &&
				cRssi >= prWifiVar->cB2RssiFactorVal3)
			u4RssiFactor = prWifiVar->ucB2RssiFactorScore3 +
				(3 * (70 + cRssi));
		else if (cRssi < prWifiVar->cB2RssiFactorVal3 &&
				cRssi >= prWifiVar->cB2RssiFactorVal4)
			u4RssiFactor = prWifiVar->ucB2RssiFactorScore4 +
				(4 * (80 + cRssi));
		else if (cRssi < prWifiVar->cB2RssiFactorVal4 &&
				cRssi >= prWifiVar->cB2RssiFactorVal5)
			u4RssiFactor = prWifiVar->ucB2RssiFactorScore5 +
				(2 * (90 + cRssi));
	}
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (bss->eBand == BAND_6G) {
		if (cRssi >= prWifiVar->cB3RssiFactorVal1)
			u4RssiFactor = prWifiVar->ucB3RssiFactorScore1;
		else if (cRssi < prWifiVar->cB3RssiFactorVal1 &&
				cRssi >= prWifiVar->cB3RssiFactorVal2)
			u4RssiFactor = prWifiVar->ucB3RssiFactorScore2 +
				(14 * (65 + cRssi));
		else if (cRssi < prWifiVar->cB3RssiFactorVal2 &&
				cRssi >= prWifiVar->cB3RssiFactorVal3)
			u4RssiFactor = prWifiVar->ucB3RssiFactorScore3 +
				(2 * (80 + cRssi));
		else if (cRssi < prWifiVar->cB3RssiFactorVal3 &&
				cRssi >= prWifiVar->cB3RssiFactorVal4)
			u4RssiFactor = prWifiVar->ucB3RssiFactorScore4 +
				2 * (90 + cRssi);
	}
#endif

	return u4RssiFactor;
}

static uint32_t apsCalculateCUFactor(
	struct ADAPTER *ad,
	struct BSS_DESC *bss,
	enum ENUM_ROAMING_REASON eRoamType,
	uint8_t bidx)
{
	uint32_t u4CUFactor = 0, u4CuRatio = 0;
	uint8_t ucChannelCuInfo = 0;
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;

	if (bss->fgExistBssLoadIE) {
		ucChannelCuInfo = bss->ucChnlUtilization;
	} else {
		ucChannelCuInfo = apsGetCuInfoEx(ad, bss, bidx);

		/* Cannot find any CU info in the same channel */
		if (ucChannelCuInfo == 0) {
			/* Apply default CU(50%) */
			ucChannelCuInfo = 128;
		}
	}

	u4CuRatio = PERCENTAGE(ucChannelCuInfo, 255);
	if (bss->eBand == BAND_2G4) {
		if (u4CuRatio < prWifiVar->ucB1CUFactorVal1)
			u4CUFactor = prWifiVar->ucB1CUFactorScore1;
		else if (u4CuRatio < prWifiVar->ucB1CUFactorVal2 &&
				u4CuRatio >= prWifiVar->ucB1CUFactorVal1)
			u4CUFactor = 111 - (13 * u4CuRatio / 10);
		else
			u4CUFactor = prWifiVar->ucB1CUFactorScore2;
	} else if (bss->eBand == BAND_5G) {
		if (u4CuRatio < prWifiVar->ucB2CUFactorVal1)
			u4CUFactor = prWifiVar->ucB2CUFactorScore1;
		else if (u4CuRatio < prWifiVar->ucB2CUFactorVal2 &&
				u4CuRatio >= prWifiVar->ucB2CUFactorVal1)
			u4CUFactor = 148 - (16 * u4CuRatio / 10);
		else
			u4CUFactor = prWifiVar->ucB2CUFactorScore2;
#if (CFG_SUPPORT_WIFI_6G == 1)
	} else if (bss->eBand == BAND_6G) {
		if (u4CuRatio < prWifiVar->ucB3CUFactorVal1)
			u4CUFactor = prWifiVar->ucB3CUFactorScore1;
		else if (u4CuRatio < prWifiVar->ucB3CUFactorVal2 &&
				u4CuRatio >= prWifiVar->ucB3CUFactorVal1)
			u4CUFactor = 180 - (20 * u4CuRatio / 10);
		else
			u4CUFactor = prWifiVar->ucB3CUFactorScore2;
#endif
	}

	return u4CUFactor;
}

uint32_t apsCalculateApScore(
	struct ADAPTER *ad,
	struct BSS_DESC *bss,
	enum ENUM_ROAMING_REASON eRoamType,
	uint8_t bidx)
{
	uint32_t u4Score = 0;
	uint8_t fgIsGBandCoex = FALSE;

	uint32_t u4RssiFactor;
	uint32_t u4CUFactor;

#if (CFG_SUPPORT_APS == 1)
	fgIsGBandCoex = aisGetApsInfo(ad, bidx)->fgIsGBandCoex;
#endif

	u4RssiFactor = apsCalculateRssiFactor(ad, bss, eRoamType, bidx) *
			ad->rWifiVar.ucRssiWeight;
	u4CUFactor = apsCalculateCUFactor(ad, bss, eRoamType, bidx) *
			ad->rWifiVar.ucCUWeight;
	u4Score = u4RssiFactor + u4CUFactor;

	/* Adjust 2.4G AP's score if BT coex */
	if (bss->eBand == BAND_2G4 && fgIsGBandCoex)
		u4Score = u4Score * ad->rWifiVar.ucRBTCScoreW / 100;

	DBGLOG(APS, INFO,
		"BSS[" MACSTR
		"] Score:%d RSSIFactor[%d] CUFactor[%d] Band[%s] GBandCoex[%d] RSSI[%d] CU[%d]",
		MAC2STR(bss->aucBSSID),
		u4Score,
		u4RssiFactor,
		u4CUFactor,
		apucBandStr[bss->eBand],
		fgIsGBandCoex,
		RCPI_TO_dBm(bss->ucRCPI),
		PERCENTAGE(bss->ucChnlUtilization, 255));

	return u4Score;
}

uint8_t apsIsBssQualify(struct ADAPTER *ad, struct BSS_DESC *bss,
	enum ENUM_ROAMING_REASON eRoamReason, uint32_t u4ConnectedApScore,
	uint32_t u4CandidateApScore, uint8_t bidx)
{
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;
	struct BSS_DESC *target_bss = aisGetTargetBssDesc(ad, bidx);

	switch (eRoamReason) {
	case ROAMING_REASON_POOR_RCPI:
	case ROAMING_REASON_RETRY:
	case ROAMING_REASON_HIGH_CU:
	{
		/* Minimum Roam Delta
		 * Absolute score value comparing to current AP
		 */
		if (u4CandidateApScore <=
			(u4ConnectedApScore + prWifiVar->ucRCMinRoamDetla)) {
			DBGLOG(APS, WARN, "BSS[" MACSTR
				"] (%d <= %d+%d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				prWifiVar->ucRCMinRoamDetla, eRoamReason);
			return FALSE;
		}

		/* Roam Delta
		 * Score percent comparing to current AP
		 */
		if ((((u4CandidateApScore - u4ConnectedApScore) * 100) /
			((u4CandidateApScore + u4ConnectedApScore) / 2)) <=
			prWifiVar->ucRCDelta) {
			DBGLOG(APS, WARN, "BSS[" MACSTR
				"] (Cand=%d,Curr=%d,delta=%d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				prWifiVar->ucRCDelta, eRoamReason);
			return FALSE;
		}
		break;
	}
	case ROAMING_REASON_BT_COEX:
	{
		/* BTCoex roaming default score delta is 0 */
		if (u4CandidateApScore < u4ConnectedApScore) {
			DBGLOG(APS, WARN, "BSS[" MACSTR
				"] (%d <= %d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				eRoamReason);
			return FALSE;
		}

		if ((((u4CandidateApScore - u4ConnectedApScore) * 100) /
			((u4CandidateApScore + u4ConnectedApScore) / 2)) <
			prWifiVar->ucRBTCDelta) {
			DBGLOG(APS, WARN, "BSS[" MACSTR
				"] (Cand=%d,Curr=%d,delta=%d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				prWifiVar->ucRBTCDelta, eRoamReason);
			return FALSE;
		}
		break;
	}
	case ROAMING_REASON_IDLE:
	{
		if (target_bss->eBand == BAND_5G && bss->eBand == BAND_2G4)
			return FALSE;

		/* Idle roaming default score delta is 0 */
		if (u4CandidateApScore < u4ConnectedApScore) {
			DBGLOG(APS, WARN, "BSS[" MACSTR
				"] (%d <= %d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				eRoamReason);
			return FALSE;
		}

		if ((((u4CandidateApScore - u4ConnectedApScore) * 100) /
			((u4CandidateApScore + u4ConnectedApScore) / 2)) <
			prWifiVar->ucRIDelta) {
			DBGLOG(APS, WARN, "BSS[" MACSTR
				"] (Cand=%d,Curr=%d,delta=%d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				prWifiVar->ucRIDelta, eRoamReason);
			return FALSE;
		}
		break;
	}
	case ROAMING_REASON_BTM:
	{
		/* BTM default score delta is 0 */
		if (u4CandidateApScore < u4ConnectedApScore) {
			DBGLOG(APS, WARN, "BSS[" MACSTR
				"] (%d <= %d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				eRoamReason);
			return FALSE;
		}

		if ((((u4CandidateApScore - u4ConnectedApScore) * 100) /
			((u4CandidateApScore + u4ConnectedApScore) / 2)) <
			prWifiVar->ucRBTMDelta) {
			DBGLOG(APS, WARN, "BSS[" MACSTR
				"] (Cand=%d,Curr=%d,delta=%d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				prWifiVar->ucRBTMDelta, eRoamReason);
			return FALSE;
		}
		break;
	}
	case ROAMING_REASON_BEACON_TIMEOUT:
	{
		/* DON'T compare score if roam with BTO */
		break;
	}
	case ROAMING_REASON_SAA_FAIL:
	{
		/* DON'T compare score if roam with emergency */
		break;
	}
	default:
		if (u4CandidateApScore < u4ConnectedApScore) {
			DBGLOG(APS, WARN, "BSS[" MACSTR
				"] (%d <= %d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				eRoamReason);
			return FALSE;
		}
		break;
	}

	return TRUE;
}
#endif

#if CFG_SUPPORT_ASSURANCE
void rrmComposeRmRejectRep(
	struct ADAPTER *prAdapter,
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRep,
	uint8_t ucToken,
	uint8_t ucMeasType,
	uint8_t ucRejectMode,
	uint8_t ucBssIndex)
{
	struct IE_MEASUREMENT_REPORT *prRepIE =
		(struct IE_MEASUREMENT_REPORT *)(prRep->pucReportFrameBuff +
						 prRep->u2ReportFrameLen);
#if CFG_SUPPORT_ASSURANCE
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
#endif

	prRepIE->ucId = ELEM_ID_MEASUREMENT_REPORT;
	prRepIE->ucToken = ucToken;
	prRepIE->ucMeasurementType = ucMeasType;
	prRepIE->ucLength = 3;
	prRepIE->ucReportMode = ucRejectMode;
	prRep->u2ReportFrameLen += 5;

#if CFG_SUPPORT_ASSURANCE
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	if (prAisSpecificBssInfo->fgBcnReptErrReasonEnable) {
		struct IE_ASSURANCE_BEACON_REPORT *ie =
			(struct IE_ASSURANCE_BEACON_REPORT *)
			prRep->pucReportFrameBuff + prRep->u2ReportFrameLen;
		ie->ucId = ELEM_ID_VENDOR;
		ie->ucLength =
		       sizeof(struct IE_ASSURANCE_BEACON_REPORT) - ELEM_HDR_LEN;
		WLAN_SET_FIELD_BE24(ie->aucOui, VENDOR_IE_SS_OUI);
		ie->ucOuiType = 0x22;
		ie->ucSubType = 0x05;
		ie->ucVersion = 0x01;
		ie->ucLen = 0x01;
		/* 0. UNSPECIFIED
		 * 1. RRM_FEATURE_DISABLED
		 * 2. NOT_SUPPORTED_PARAMETERS
		 * 3. TEMPORARILY_UNAVAILABLE
		 * 4. VALIDATION_FAILED_IN_A_REQUEST_FRAME
		 * 5. PREVIOUS_REQUEST_PROGRESS
		 * 6. MAXIMUM_MEASUREMENT_DURATION_EXCEED
		 * 7. NOT_SUPPORTED_REPORT_BITS
		 */
		ie->ucReason = 2;
		prRep->u2ReportFrameLen += IE_SIZE(ie);
	}
#endif
}

void kalUpdateDeauthInfo(
	struct GLUE_INFO *prGlueInfo,
	uint8_t *pucFrameBody,
	uint32_t u4FrameBodyLen,
	uint8_t ucBssIndex)
{
	uint32_t u4IEOffset = 2;      /* ReasonCode */
	uint32_t u4IELength = u4FrameBodyLen - u4IEOffset;
	struct BSS_INFO *prAisBssInfo;

	ASSERT(prGlueInfo);

	prAisBssInfo = aisGetAisBssInfo(
		prGlueInfo->prAdapter,
		ucBssIndex);

	/* reset */
	prAisBssInfo->u4DeauthIeLength = 0;

	if (u4IELength <= CFG_CFG80211_IE_BUF_LEN) {
		prAisBssInfo->u4DeauthIeLength = u4IELength;
		kalMemCopy(prAisBssInfo->aucDeauthIe,
			pucFrameBody + u4IEOffset,
			u4IELength);
	}
}

void deauth_build_nonwfa_vend_ie(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	uint8_t *ptr = NULL;
	uint16_t len = 0;

	if (!prAdapter || !prMsduInfo)
		return;

	len = prAdapter->u4DeauthIeFromUpperLength;
	if (!len)
		return;

	DBGLOG(SAA, INFO, "send nonwfa vendor IE, IeLen=%d\n", len);
	ptr = (uint8_t *)prMsduInfo->prPacket +
		(uint16_t)prMsduInfo->u2FrameLength;
	kalMemCopy(ptr, prAdapter->aucDeauthIeFromUpper, len);
	prMsduInfo->u2FrameLength += len;
}

uint32_t assocCalculateRoamReasonLen(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIdx,
	struct STA_RECORD *prStaRec)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	uint8_t ucBssIndex = 0;

	ucBssIndex = prStaRec->ucBssIndex;
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	if (IS_STA_IN_AIS(prStaRec) && prStaRec->fgIsReAssoc &&
	    prAisSpecificBssInfo->fgRoamingReasonEnable)
		return sizeof(struct IE_ASSURANCE_ROAMING_REASON);

	return 0;
}

void assocGenerateRoamReason(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	struct ROAMING_INFO *prRoamingFsmInfo = NULL;
	struct STA_RECORD *prStaRec;
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;
	struct BSS_INFO *prAisBssInfo;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	if (!prStaRec)
		return;

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	ucBssIndex = prStaRec->ucBssIndex;
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prRoamingFsmInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

	if (IS_STA_IN_AIS(prStaRec) && prStaRec->fgIsReAssoc &&
	    prAisSpecificBssInfo->fgRoamingReasonEnable) {
		struct IE_ASSURANCE_ROAMING_REASON *ie =
			(struct IE_ASSURANCE_ROAMING_REASON *) pucBuffer;

		ie->ucId = ELEM_ID_VENDOR;
		ie->ucLength =
		      sizeof(struct IE_ASSURANCE_ROAMING_REASON) - ELEM_HDR_LEN;
		WLAN_SET_FIELD_BE24(ie->aucOui, VENDOR_IE_SS_OUI);
		ie->ucOuiType = 0x22;
		ie->ucSubType = 0x04;
		ie->ucVersion = 0x01;
		ie->ucSubTypeReason = 0x00;

		/* 0. unspecified
		 * 1. low rssi
		 * 2. CU
		 * 3. Beacon lost
		 * 4. Deauth/Disassoc
		 * 5. BTM
		 * 6. idle roaming
		 * 7. manual
		 */
		switch (prRoamingFsmInfo->eReason) {
		case ROAMING_REASON_POOR_RCPI:
			ie->ucReason = 0x01;
			break;
		case ROAMING_REASON_BEACON_TIMEOUT:
			ie->ucReason = 0x03;
			break;
		case ROAMING_REASON_SAA_FAIL:
			ie->ucReason = 0x04;
			break;
		case ROAMING_REASON_BTM:
			ie->ucReason = 0x05;
			break;
		case ROAMING_REASON_INACTIVE:
		case ROAMING_REASON_TX_ERR:
			ie->ucReason = 0x07;
			break;
		default:
			ie->ucReason = 0x05;
		}
		ie->ucSubTypeRcpi = 0x01;
		ie->ucRcpi = prRoamingFsmInfo->ucRcpi;
		ie->ucSubTypeRcpiThreshold = 0x02;
		ie->ucRcpiThreshold = prRoamingFsmInfo->ucThreshold;
		ie->ucSubTypeCuThreshold = 0x03;
		ie->ucCuThreshold = 0;
		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
		DBGLOG_MEM8(SAA, INFO, pucBuffer, IE_SIZE(pucBuffer));
	}
}

uint32_t
wlanoidSetDisconnectIes(struct ADAPTER *prAdapter,
			 void *pvSetBuffer,
			 uint32_t u4SetBufferLen,
			 uint32_t *pu4SetInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	if (u4SetBufferLen <= NON_WFA_VENDOR_IE_MAX_LEN) {
		prAdapter->u4DeauthIeFromUpperLength = u4SetBufferLen;
		kalMemCopy(prAdapter->aucDeauthIeFromUpper,
			pvSetBuffer, u4SetBufferLen);
		DBGLOG_MEM8(REQ, INFO, (uint8_t *)
			prAdapter->aucDeauthIeFromUpper, u4SetBufferLen);
		rStatus = WLAN_STATUS_SUCCESS;
	}

	return rStatus;
}

uint32_t
wlanoidSetRoamingReasonEnable(struct ADAPTER *prAdapter,
			 void *pvSetBuffer,
			 uint32_t u4SetBufferLen,
			 uint32_t *pu4SetInfoLen)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	uint8_t ucBssIndex = 0;
	uint32_t *pu4Param = NULL;

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pu4Param = (uint32_t *) pvSetBuffer;
	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prAisSpecificBssInfo->fgRoamingReasonEnable = *pu4Param;

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidGetRoamingReasonEnable(struct ADAPTER *prAdapter,
			   void *pvQueryBuffer,
			   uint32_t u4QueryBufferLen,
			   uint32_t *pu4QueryInfoLen)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	uint8_t ucBssIndex = 0;
	uint32_t *pu4Param = NULL;

	*pu4QueryInfoLen = sizeof(uint32_t);
	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pu4Param = (uint32_t *) pvQueryBuffer;
	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	*pu4Param = prAisSpecificBssInfo->fgRoamingReasonEnable;

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetBrErrReasonEnable(struct ADAPTER *prAdapter,
			 void *pvSetBuffer,
			 uint32_t u4SetBufferLen,
			 uint32_t *pu4SetInfoLen)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	uint8_t ucBssIndex = 0;
	uint32_t *pu4Param = NULL;

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pu4Param = (uint32_t *) pvSetBuffer;
	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	prAisSpecificBssInfo->fgBcnReptErrReasonEnable = *pu4Param;

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidGetBrErrReasonEnable(struct ADAPTER *prAdapter,
			   void *pvQueryBuffer,
			   uint32_t u4QueryBufferLen,
			   uint32_t *pu4QueryInfoLen)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo;
	uint8_t ucBssIndex = 0;
	uint32_t *pu4Param = NULL;

	*pu4QueryInfoLen = sizeof(uint32_t);
	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pu4Param = (uint32_t *) pvQueryBuffer;
	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	*pu4Param = prAisSpecificBssInfo->fgBcnReptErrReasonEnable;

	return WLAN_STATUS_SUCCESS;
}
#endif

#if CFG_SUPPORT_NCHO
uint32_t wlanNchoSetFWEnable(struct ADAPTER *prAdapter, uint8_t fgEnable)
{
	char cmd[NCHO_CMD_MAX_LENGTH] = { 0 };
	uint32_t status = WLAN_STATUS_FAILURE;

	kalSnprintf(cmd, sizeof(cmd), "%s %d",
		FW_CFG_KEY_NCHO_ENABLE, fgEnable);
	status = wlanFwCfgParse(prAdapter, cmd);
	if (status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, WARN, "set disable fail %d\n", status);
	return status;
}

uint32_t wlanNchoSetFWScanPeriod(struct ADAPTER *prAdapter,
	uint32_t u4RoamScanPeriod)
{
	char cmd[NCHO_CMD_MAX_LENGTH] = { 0 };
	uint32_t status = WLAN_STATUS_FAILURE;

	kalSnprintf(cmd, sizeof(cmd), "%s %d",
		FW_CFG_KEY_NCHO_SCAN_PERIOD, u4RoamScanPeriod);
	status = wlanFwCfgParse(prAdapter, cmd);
	if (status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, WARN, "set scan period fail %d\n", status);
	return status;
}

void wlanNchoInit(struct ADAPTER *prAdapter, uint8_t fgFwSync)
{
	uint8_t sync = fgFwSync && prAdapter->rNchoInfo.fgNCHOEnabled;

	/* NCHO Initialization */
	prAdapter->rNchoInfo.fgNCHOEnabled = 0;
	prAdapter->rNchoInfo.ucBand = NCHO_BAND_LIST_2G4 | NCHO_BAND_LIST_5G;
	prAdapter->rNchoInfo.fgChGranted = FALSE;
	prAdapter->rNchoInfo.fgIsSendingAF = FALSE;
	prAdapter->rNchoInfo.u4RoamScanControl = 0;
	prAdapter->rNchoInfo.rRoamScnChnl.ucChannelListNum = 0;
	prAdapter->rNchoInfo.rAddRoamScnChnl.ucChannelListNum = 0;
	prAdapter->rNchoInfo.eDFSScnMode = NCHO_DFS_SCN_ENABLE1;
	prAdapter->rNchoInfo.i4RoamTrigger = -75;
	prAdapter->rNchoInfo.i4RoamDelta = 10;
	prAdapter->rNchoInfo.u4RoamScanPeriod = ROAMING_DISCOVER_TIMEOUT_SEC;
	prAdapter->rNchoInfo.u4ScanChannelTime = 50;
	prAdapter->rNchoInfo.u4ScanHomeTime = 120;
	prAdapter->rNchoInfo.u4ScanHomeawayTime = 120;
	prAdapter->rNchoInfo.u4ScanNProbes = 2;
	prAdapter->rNchoInfo.u4WesMode = 0;
	prAdapter->rAddRoamScnChnl.ucChannelListNum = 0;
	prAdapter->rNchoInfo.eRoamBand = 0;
	prAdapter->rNchoInfo.ucRoamBand =
		(NCHO_BAND_LIST_2G4 | NCHO_BAND_LIST_5G
#if (CFG_SUPPORT_WIFI_6G == 1)
		| NCHO_BAND_LIST_6G
#endif
		);

	/* sync with FW to let FW reset params */
	if (sync)
		wlanNchoSetFWEnable(prAdapter, 0);
}

uint32_t wlanSetFWRssiTrigger(
	struct ADAPTER *prAdapter,
	const char *key,
	int32_t i4RoamTriggerRssi)
{
	char cmd[NCHO_CMD_MAX_LENGTH] = { 0 };
	uint32_t status = WLAN_STATUS_FAILURE;

	kalSnprintf(cmd, sizeof(cmd), "%s %d",
		key, dBm_TO_RCPI(i4RoamTriggerRssi));
	status =  wlanFwCfgParse(prAdapter, cmd);
	if (status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, WARN, "set roam rcpi fail %d\n", status);

	return status;
}

uint32_t
wlanoidSetNchoHeader(struct CMD_HEADER *prCmdHeader,
		     struct CMD_FORMAT_V1 *pr_cmd_v1,
		     char *pStr, uint32_t u4Len) {

	if (!prCmdHeader || !pStr || u4Len == 0)
		return WLAN_STATUS_FAILURE;

	prCmdHeader->cmdVersion = CMD_VER_1_EXT;
	prCmdHeader->cmdType = CMD_TYPE_QUERY;
	prCmdHeader->itemNum = 1;
	prCmdHeader->cmdBufferLen = sizeof(struct CMD_FORMAT_V1);
	kalMemSet(prCmdHeader->buffer, 0, MAX_CMD_BUFFER_LENGTH);

	pr_cmd_v1->itemStringLength = u4Len;
	kalMemCopy(pr_cmd_v1->itemString, pStr, u4Len);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoRoamTrigger(struct ADAPTER *prAdapter,
			  void *pvSetBuffer, uint32_t u4SetBufferLen,
			  uint32_t *pu4SetInfoLen) {
	int32_t *pi4Param = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(int32_t);

	if (u4SetBufferLen < sizeof(int32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pi4Param = (int32_t *) pvSetBuffer;
	if (*pi4Param < RCPI_TO_dBm(RCPI_LOW_BOUND)
	    || *pi4Param > RCPI_TO_dBm(RCPI_HIGH_BOUND)) {
		DBGLOG(INIT, ERROR, "NCHO roam trigger invalid %d\n",
		       *pi4Param);
		return WLAN_STATUS_INVALID_DATA;
	}

	rStatus = wlanSetFWRssiTrigger(prAdapter,
		FW_CFG_KEY_NCHO_ROAM_RCPI, *pi4Param);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.i4RoamTrigger = *pi4Param;
		DBGLOG(INIT, TRACE, "NCHO roam trigger is %d\n",
		       prAdapter->rNchoInfo.i4RoamTrigger);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamTrigger(struct ADAPTER *prAdapter,
			    void *pvQueryBuffer,
			    uint32_t u4QueryBufferLen,
			    uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
				       prCmdV1,
				       FW_CFG_KEY_NCHO_ROAM_RCPI,
				       kalStrLen(FW_CFG_KEY_NCHO_ROAM_RCPI));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  TRUE,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoRoamDelta(struct ADAPTER *prAdapter,
			void *pvSetBuffer, uint32_t u4SetBufferLen,
			uint32_t *pu4SetInfoLen) {
	int32_t *pi4Param = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(int32_t);

	if (u4SetBufferLen < sizeof(int32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pi4Param = (int32_t *) pvSetBuffer;
	if (*pi4Param > 100) {
		DBGLOG(INIT, ERROR, "NCHO roam delta invalid %d\n",
		       *pi4Param);
		return WLAN_STATUS_INVALID_DATA;
	}

	prAdapter->rNchoInfo.i4RoamDelta = *pi4Param;
	DBGLOG(INIT, TRACE, "NCHO roam delta is %d\n", *pi4Param);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamDelta(struct ADAPTER *prAdapter,
			  void *pvQueryBuffer, uint32_t u4QueryBufferLen,
			  uint32_t *pu4QueryInfoLen) {
	int32_t *pParam = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(int32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (int32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.i4RoamDelta;
	DBGLOG(INIT, TRACE, "NCHO roam delta is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoRoamScnPeriod(struct ADAPTER *prAdapter,
			    void *pvSetBuffer, uint32_t u4SetBufferLen,
			    uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *) pvSetBuffer;
	rStatus = wlanNchoSetFWScanPeriod(prAdapter, *pParam);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4RoamScanPeriod = *pParam;
		DBGLOG(INIT, TRACE, "NCHO roam scan period is %d\n", *pParam);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamScnPeriod(struct ADAPTER *prAdapter,
			      void *pvQueryBuffer,
			      uint32_t u4QueryBufferLen,
			      uint32_t *pu4QueryInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
					prCmdV1,
					FW_CFG_KEY_NCHO_SCAN_PERIOD,
					kalStrLen(FW_CFG_KEY_NCHO_SCAN_PERIOD));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header, sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			prAdapter,
			CMD_ID_GET_SET_CUSTOMER_CFG,
			FALSE,
			TRUE,
			TRUE,
			nicCmdEventQueryCfgRead,
			nicOidCmdTimeoutCommon,
			sizeof(struct CMD_HEADER),
			(uint8_t *)&cmdV1Header,
			pvQueryBuffer,
			u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoRoamScnChnl(struct ADAPTER *prAdapter,
			  void *pvSetBuffer, uint32_t u4SetBufferLen,
			  uint32_t *pu4SetInfoLen)
{
	struct CFG_NCHO_SCAN_CHNL *prRoamScnChnl = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(struct CFG_NCHO_SCAN_CHNL);

	if (u4SetBufferLen < sizeof(struct CFG_NCHO_SCAN_CHNL))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prRoamScnChnl = (struct CFG_NCHO_SCAN_CHNL *) pvSetBuffer;

	kalMemCopy(&prAdapter->rNchoInfo.rRoamScnChnl,
		   prRoamScnChnl, *pu4SetInfoLen);
	DBGLOG(INIT, TRACE,
	       "NCHO set roam scan channel num is %d\n",
	       prRoamScnChnl->ucChannelListNum);

	prAdapter->rNchoInfo.u4RoamScanControl = 1;

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidAddNchoRoamScnChnl(struct ADAPTER *prAdapter,
			  void *pvSetBuffer, uint32_t u4SetBufferLen,
			  uint32_t *pu4SetInfoLen)
{
	struct CFG_NCHO_SCAN_CHNL *prRoamScnChnl = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(struct CFG_NCHO_SCAN_CHNL);

	if (u4SetBufferLen < sizeof(struct CFG_NCHO_SCAN_CHNL))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prRoamScnChnl = (struct CFG_NCHO_SCAN_CHNL *) pvSetBuffer;

	kalMemCopy(&prAdapter->rNchoInfo.rAddRoamScnChnl,
		   prRoamScnChnl, *pu4SetInfoLen);
	DBGLOG(INIT, TRACE,
	       "NCHO set roam scan channel num is %d\n",
	       prRoamScnChnl->ucChannelListNum);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidQueryNchoRoamScnChnl(struct ADAPTER *prAdapter,
			    void *pvQueryBuffer,
			    uint32_t u4QueryBufferLen,
			    uint32_t *pu4QueryInfoLen)
{
	struct CFG_NCHO_SCAN_CHNL *prRoamScnChnl = NULL;
	struct CFG_NCHO_SCAN_CHNL *chnl;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(struct CFG_NCHO_SCAN_CHNL))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prRoamScnChnl = (struct CFG_NCHO_SCAN_CHNL *) pvQueryBuffer;

	if (prAdapter->rNchoInfo.u4RoamScanControl)
		chnl = &prAdapter->rNchoInfo.rRoamScnChnl;
	else
		chnl = &prAdapter->rNchoInfo.rAddRoamScnChnl;

	kalMemCopy(prRoamScnChnl, chnl, u4QueryBufferLen);
	DBGLOG(INIT, TRACE, "NCHO roam scan channel num is %d, ctrl %d\n",
	       prRoamScnChnl->ucChannelListNum,
	       prAdapter->rNchoInfo.u4RoamScanControl);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoRoamScnCtrl(struct ADAPTER *prAdapter,
			  void *pvSetBuffer, uint32_t u4SetBufferLen,
			  uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam != TRUE && *pParam != FALSE) {
		DBGLOG(INIT, ERROR, "NCHO roam scan control invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	prAdapter->rNchoInfo.u4RoamScanControl = *pParam;
	DBGLOG(INIT, TRACE, "NCHO roam scan control is %d\n",
	       *pParam);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamScnCtrl(struct ADAPTER *prAdapter,
			    void *pvQueryBuffer,
			    uint32_t u4QueryBufferLen,
			    uint32_t *pu4QueryInfoLen) {
	uint32_t *pParam = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.u4RoamScanControl;
	DBGLOG(INIT, TRACE, "NCHO roam scan control is %d\n",
	       *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoScnChnlTime(struct ADAPTER *prAdapter,
			  void *pvSetBuffer, uint32_t u4SetBufferLen,
			  uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam < 10 && *pParam > 1000) {
		DBGLOG(INIT, ERROR, "NCHO scan channel time invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME,
		   *pParam);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanChannelTime = *pParam;
		DBGLOG(INIT, TRACE, "NCHO scan channel time is %d\n",
		       *pParam);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoScnChnlTime(struct ADAPTER *prAdapter,
			    void *pvQueryBuffer,
			    uint32_t u4QueryBufferLen,
			    uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header, prCmdV1,
			       FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME,
			       kalStrLen(FW_CFG_KEY_NCHO_SCN_CHANNEL_TIME));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  TRUE,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoScnHomeTime(struct ADAPTER *prAdapter,
			  void *pvSetBuffer, uint32_t u4SetBufferLen,
			  uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam < 10 && *pParam > 1000) {
		DBGLOG(INIT, ERROR, "NCHO scan home time invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_HOME_TIME,
		   *pParam);
	DBGLOG(REQ, TRACE, "NCHO cmd is %s\n", acCmd);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanHomeTime = *pParam;
		DBGLOG(INIT, TRACE, "NCHO scan home time is %d\n", *pParam);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoScnHomeTime(struct ADAPTER *prAdapter,
			    void *pvQueryBuffer,
			    uint32_t u4QueryBufferLen,
			    uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header, prCmdV1,
			       FW_CFG_KEY_NCHO_SCN_HOME_TIME,
			       kalStrLen(FW_CFG_KEY_NCHO_SCN_HOME_TIME));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  TRUE,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoScnHomeAwayTime(struct ADAPTER *prAdapter,
			      void *pvSetBuffer, uint32_t u4SetBufferLen,
			      uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam < 10 && *pParam > 1000) {
		DBGLOG(INIT, ERROR, "NCHO scan home away time invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	kalSprintf(acCmd, "%s %d",
		   FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME, *pParam);
	DBGLOG(REQ, TRACE, "NCHO cmd is %s\n", acCmd);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanHomeawayTime = *pParam;
		DBGLOG(INIT, TRACE, "NCHO scan home away is %d\n", *pParam);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoScnHomeAwayTime(struct ADAPTER *prAdapter,
				void *pvQueryBuffer,
				uint32_t u4QueryBufferLen,
				uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header, prCmdV1,
			       FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME,
			       kalStrLen(FW_CFG_KEY_NCHO_SCN_HOME_AWAY_TIME));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  TRUE,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoScnNprobes(struct ADAPTER *prAdapter,
			 void *pvSetBuffer, uint32_t u4SetBufferLen,
			 uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam > 16) {
		DBGLOG(INIT, ERROR, "NCHO scan Nprobes invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCN_NPROBES,
		   *pParam);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.u4ScanNProbes = *pParam;
		DBGLOG(INIT, TRACE, "NCHO Nprobes is %d\n", *pParam);
	}
	return rStatus;
}

uint32_t
wlanoidQueryNchoScnNprobes(struct ADAPTER *prAdapter,
			   void *pvQueryBuffer,
			   uint32_t u4QueryBufferLen,
			   uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
				       prCmdV1,
				       FW_CFG_KEY_NCHO_SCN_NPROBES,
				       kalStrLen(FW_CFG_KEY_NCHO_SCN_NPROBES));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  TRUE,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidGetNchoReassocInfo(struct ADAPTER *prAdapter,
			  void *pvQueryBuffer,
			  uint32_t u4QueryBufferLen,
			  uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct BSS_DESC *prBssDesc = NULL;
	struct PARAM_CONNECT *prParamConn;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	ASSERT(pvQueryBuffer);

	prParamConn = (struct PARAM_CONNECT *)pvQueryBuffer;
	if (prAdapter->rNchoInfo.fgNCHOEnabled == TRUE) {
		prBssDesc = scanSearchBssDescByBssid(prAdapter,
						     prParamConn->pucBssid);
		if (prBssDesc != NULL) {
			prParamConn->u4SsidLen = prBssDesc->ucSSIDLen;
			COPY_SSID(prParamConn->pucSsid,
				  prParamConn->u4SsidLen,
				  prBssDesc->aucSSID,
				  prBssDesc->ucSSIDLen);
			rStatus = WLAN_STATUS_SUCCESS;
		}
	}
	return rStatus;
}

uint32_t
wlanoidSendNchoActionFrameStart(struct ADAPTER *prAdapter,
				void *pvSetBuffer,
				uint32_t u4SetBufferLen,
				uint32_t *pu4SetInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct NCHO_INFO *prNchoInfo = NULL;
	struct NCHO_ACTION_FRAME_PARAMS *prParamActionFrame = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);


	prNchoInfo = &prAdapter->rNchoInfo;
	prParamActionFrame = (struct NCHO_ACTION_FRAME_PARAMS *)pvSetBuffer;
	prNchoInfo->fgIsSendingAF = TRUE;
	prNchoInfo->fgChGranted = FALSE;
	COPY_MAC_ADDR(prNchoInfo->rParamActionFrame.aucBssid,
					prParamActionFrame->aucBssid);
	prNchoInfo->rParamActionFrame.i4channel = prParamActionFrame->i4channel;
	prNchoInfo->rParamActionFrame.i4DwellTime =
					prParamActionFrame->i4DwellTime;
	prNchoInfo->rParamActionFrame.i4len = prParamActionFrame->i4len;
	kalMemCopy(prNchoInfo->rParamActionFrame.aucData,
		   prParamActionFrame->aucData,
		   prParamActionFrame->i4len);
	DBGLOG(INIT, TRACE, "NCHO send ncho action frame start\n");
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidSendNchoActionFrameEnd(struct ADAPTER *prAdapter,
			      void *pvSetBuffer, uint32_t u4SetBufferLen,
			      uint32_t *pu4SetInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	prAdapter->rNchoInfo.fgIsSendingAF = FALSE;
	prAdapter->rNchoInfo.fgChGranted = TRUE;
	DBGLOG(INIT, TRACE, "NCHO send action frame end\n");
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidSetNchoWesMode(struct ADAPTER *prAdapter,
		      void *pvSetBuffer, uint32_t u4SetBufferLen,
		      uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam != TRUE && *pParam != FALSE) {
		DBGLOG(INIT, ERROR, "NCHO wes mode invalid %d\n", *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	prAdapter->rNchoInfo.u4WesMode = *pParam;
	DBGLOG(INIT, TRACE, "NCHO WES mode is %d\n", *pParam);
	rStatus = WLAN_STATUS_SUCCESS;

	return rStatus;
}

uint32_t
wlanoidQueryNchoWesMode(struct ADAPTER *prAdapter,
			void *pvQueryBuffer, uint32_t u4QueryBufferLen,
			uint32_t *pu4QueryInfoLen) {
	uint32_t *pParam = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.u4WesMode;
	DBGLOG(INIT, TRACE, "NCHO Wes mode is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoBand(struct ADAPTER *prAdapter,
		   void *pvSetBuffer, uint32_t u4SetBufferLen,
		   uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct BSS_INFO *ais = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;

	switch (*pParam) {
	case NCHO_BAND_AUTO_2G4_5G:
		prAdapter->rNchoInfo.ucBand =
			(NCHO_BAND_LIST_2G4 | NCHO_BAND_LIST_5G);
		break;
	case NCHO_BAND_2G4:
		prAdapter->rNchoInfo.ucBand = NCHO_BAND_LIST_2G4;
		break;
	case NCHO_BAND_5G:
		prAdapter->rNchoInfo.ucBand = NCHO_BAND_LIST_5G;
		break;
#if (CFG_SUPPORT_WIFI_6G == 1)
	case NCHO_BAND_AUTO_2G4_5G_6G:
		prAdapter->rNchoInfo.ucBand =
		(NCHO_BAND_LIST_2G4 | NCHO_BAND_LIST_5G | NCHO_BAND_LIST_6G);
		break;
	case NCHO_BAND_6G:
		prAdapter->rNchoInfo.ucBand = NCHO_BAND_LIST_6G;
		break;
	case NCHO_BAND_5G_6G:
		prAdapter->rNchoInfo.ucBand =
			(NCHO_BAND_LIST_5G | NCHO_BAND_LIST_6G);
		break;
	case NCHO_BAND_2G4_6G:
		prAdapter->rNchoInfo.ucBand =
			(NCHO_BAND_LIST_6G | NCHO_BAND_LIST_2G4);
		break;
#endif
	default:
		DBGLOG(INIT, ERROR, "NCHO invalid band: %d\n", *pParam);
		rStatus = WLAN_STATUS_INVALID_DATA;
		break;
	}
	prAdapter->rNchoInfo.eCongfigBand = *pParam;

	DBGLOG(INIT, INFO, "NCHO enabled:%d ,band:%d,status:%d\n"
	       , prAdapter->rNchoInfo.fgNCHOEnabled, *pParam, rStatus);

	/* Execute disconnect process if current band is not equal to config */
	ais = aisGetConnectedBssInfo(prAdapter);
	if (ais && !(BIT(ais->eBand) & prAdapter->rNchoInfo.ucBand))
		aisFsmStateAbort(prAdapter,
				DISCONNECT_REASON_CODE_NCHO_DIFF_BAND,
				FALSE, AIS_DEFAULT_INDEX);

	return rStatus;
}

uint32_t
wlanoidQueryNchoBand(struct ADAPTER *prAdapter,
		     void *pvQueryBuffer, uint32_t u4QueryBufferLen,
		     uint32_t *pu4QueryInfoLen) {
	uint32_t *pParam = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.eCongfigBand;
	DBGLOG(INIT, TRACE, "NCHO band is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoDfsScnMode(struct ADAPTER *prAdapter,
			 void *pvSetBuffer, uint32_t u4SetBufferLen,
			 uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	char acCmd[NCHO_CMD_MAX_LENGTH] = {0};
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam >= NCHO_DFS_SCN_NUM) {
		DBGLOG(INIT, ERROR, "NCHO DFS scan mode invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}


	kalSprintf(acCmd, "%s %d", FW_CFG_KEY_NCHO_SCAN_DFS_MODE,
		   *pParam);
	rStatus =  wlanFwCfgParse(prAdapter, acCmd);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prAdapter->rNchoInfo.eDFSScnMode = *pParam;
		DBGLOG(INIT, TRACE, "NCHO DFS scan mode is %d\n", *pParam);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoDfsScnMode(struct ADAPTER *prAdapter,
			   void *pvQueryBuffer,
			   uint32_t u4QueryBufferLen,
			   uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header, prCmdV1,
			       FW_CFG_KEY_NCHO_SCAN_DFS_MODE,
			       kalStrLen(FW_CFG_KEY_NCHO_SCAN_DFS_MODE));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  TRUE,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidSetNchoEnable(struct ADAPTER *prAdapter,
		     void *pvSetBuffer, uint32_t u4SetBufferLen,
		     uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint8_t ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);

	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pParam = (uint32_t *) pvSetBuffer;
	if (*pParam != 0 && *pParam != 1) {
		DBGLOG(INIT, ERROR, "NCHO DFS scan mode invalid %d\n",
		       *pParam);
		return WLAN_STATUS_INVALID_DATA;
	}

	rStatus = wlanNchoSetFWEnable(prAdapter, *pParam);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		char aucLog[256] = {0};

		wlanNchoInit(prAdapter, FALSE);
		prAdapter->rNchoInfo.fgNCHOEnabled = *pParam;
		DBGLOG(INIT, INFO, "NCHO enable is %d\n", *pParam);

		kalSprintf(aucLog, "[NCHO] MODE enable=%d", *pParam);
		kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
	}

	return rStatus;
}

uint32_t
wlanoidQueryNchoEnable(struct ADAPTER *prAdapter,
		       void *pvQueryBuffer, uint32_t u4QueryBufferLen,
		       uint32_t *pu4QueryInfoLen) {
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
				       prCmdV1,
				       FW_CFG_KEY_NCHO_ENABLE,
				       kalStrLen(FW_CFG_KEY_NCHO_ENABLE));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "NCHO no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  TRUE,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidQueryNchoRoamBand(struct ADAPTER *prAdapter,
		     void *pvQueryBuffer, uint32_t u4QueryBufferLen,
		     uint32_t *pu4QueryInfoLen) {
	uint32_t *pParam = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *) pvQueryBuffer;
	*pParam = prAdapter->rNchoInfo.eRoamBand;
	DBGLOG(INIT, TRACE, "NCHO roam band is %d\n", *pParam);

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetNchoRoamBand(struct ADAPTER *prAdapter,
		     void *pvSetBuffer, uint32_t u4SetBufferLen,
		     uint32_t *pu4SetInfoLen) {
	uint32_t *pParam = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(OID, LOUD, "\n");

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(uint32_t);

	if (u4SetBufferLen < sizeof(uint32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	if (prAdapter->rNchoInfo.fgNCHOEnabled == FALSE)
		return WLAN_STATUS_INVALID_DATA;

	pParam = (uint32_t *)pvSetBuffer;

	switch (*pParam) {
	case NCHO_ROAM_BAND_AUTO:
		prAdapter->rNchoInfo.ucRoamBand =
			(NCHO_BAND_LIST_2G4 | NCHO_BAND_LIST_5G
#if (CFG_SUPPORT_WIFI_6G == 1)
			| NCHO_BAND_LIST_6G
#endif
			);
		break;
	case NCHO_ROAM_BAND_2G4:
		prAdapter->rNchoInfo.ucRoamBand = NCHO_BAND_LIST_2G4;
		break;
	case NCHO_ROAM_BAND_5G:
		prAdapter->rNchoInfo.ucRoamBand = NCHO_BAND_LIST_5G;
		break;
	case NCHO_ROAM_BAND_2G4_5G:
		prAdapter->rNchoInfo.ucRoamBand =
			(NCHO_BAND_LIST_2G4 | NCHO_BAND_LIST_5G);
		break;
#if (CFG_SUPPORT_WIFI_6G == 1)
	case NCHO_ROAM_BAND_6G:
		prAdapter->rNchoInfo.ucRoamBand = NCHO_BAND_LIST_6G;
		break;
	case NCHO_ROAM_BAND_2G4_6G:
		prAdapter->rNchoInfo.ucRoamBand =
			(NCHO_BAND_LIST_2G4 | NCHO_BAND_LIST_6G);
		break;
	case NCHO_ROAM_BAND_5G_6G:
		prAdapter->rNchoInfo.ucRoamBand =
			(NCHO_BAND_LIST_5G | NCHO_BAND_LIST_6G);
		break;
	case NCHO_ROAM_BAND_2G4_5g_6G:
		prAdapter->rNchoInfo.ucRoamBand =
			(NCHO_BAND_LIST_6G | NCHO_BAND_LIST_2G4 |
			NCHO_BAND_LIST_5G);
		break;
#endif
	default:
		DBGLOG(INIT, ERROR, "NCHO invalid roam band %d\n", *pParam);
		rStatus = WLAN_STATUS_INVALID_DATA;
		break;
	}

	prAdapter->rNchoInfo.eRoamBand = *pParam;

	return rStatus;
}

uint32_t
wlanoidQueryRoamScnChnl(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen)
{
	struct CFG_SCAN_CHNL *list = NULL;
	struct AIS_SPECIFIC_BSS_INFO *ais;
	uint8_t i = 0;
	uint8_t ucBssIndex = 0;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	if (u4QueryBufferLen < sizeof(struct CFG_SCAN_CHNL))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	list = (struct CFG_NCHO_SCAN_CHNL *) pvQueryBuffer;
	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	ais = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);

	list->ucChannelListNum = ais->ucCurEssChnlInfoNum;
	for (i = 0; i < ais->ucCurEssChnlInfoNum; i++)
		list->arChnlInfoList[i].ucChannelNum =
			ais->arCurEssChnlInfo[i].ucChannel;

	DBGLOG(INIT, TRACE, "Roam scan channel num is %d\n",
			       list->ucChannelListNum);
	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidSetRoamTrigger(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	int32_t *pi4Param = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = sizeof(int32_t);

	if (u4SetBufferLen < sizeof(int32_t))
		return WLAN_STATUS_INVALID_LENGTH;

	pi4Param = (int32_t *) pvSetBuffer;
	if (dBm_TO_RCPI(*pi4Param) <= RCPI_LOW_BOUND ||
	    dBm_TO_RCPI(*pi4Param) >= RCPI_HIGH_BOUND) {
		DBGLOG(INIT, ERROR, "roam trigger invalid %d\n",
		       *pi4Param);
		return WLAN_STATUS_INVALID_DATA;
	}

	rStatus = wlanSetFWRssiTrigger(prAdapter,
		FW_CFG_KEY_ROAM_RCPI, *pi4Param);
	if (rStatus == WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "roam trigger is %d\n", *pi4Param);

	return rStatus;
}

uint32_t
wlanoidQueryRoamTrigger(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct CMD_HEADER *prCmdV1Header = (struct CMD_HEADER *)
					   pvQueryBuffer;
	struct CMD_FORMAT_V1 *prCmdV1 = NULL;

	ASSERT(prAdapter);
	ASSERT(pu4QueryInfoLen);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);

	*pu4QueryInfoLen = sizeof(struct CMD_HEADER);

	if (u4QueryBufferLen < sizeof(struct CMD_HEADER))
		return WLAN_STATUS_BUFFER_TOO_SHORT;

	prCmdV1 = (struct CMD_FORMAT_V1 *) prCmdV1Header->buffer;
	rStatus = wlanoidSetNchoHeader(prCmdV1Header,
				       prCmdV1,
				       FW_CFG_KEY_ROAM_RCPI,
				       kalStrLen(FW_CFG_KEY_ROAM_RCPI));
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "no enough memory\n");
		return rStatus;
	}
	kalMemCopy(&cmdV1Header, prCmdV1Header,
		   sizeof(struct CMD_HEADER));
	rStatus = wlanSendSetQueryCmd(
			  prAdapter,
			  CMD_ID_GET_SET_CUSTOMER_CFG,
			  FALSE,
			  TRUE,
			  TRUE,
			  nicCmdEventQueryCfgRead,
			  nicOidCmdTimeoutCommon,
			  sizeof(struct CMD_HEADER),
			  (uint8_t *)&cmdV1Header,
			  pvQueryBuffer,
			  u4QueryBufferLen);
	return rStatus;
}

uint32_t
wlanoidAddRoamScnChnl(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	struct CFG_SCAN_CHNL *prRoamScnChnl = NULL;

	*pu4SetInfoLen = sizeof(struct CFG_SCAN_CHNL);

	if (u4SetBufferLen < sizeof(struct CFG_SCAN_CHNL))
		return WLAN_STATUS_INVALID_LENGTH;

	prRoamScnChnl = (struct CFG_SCAN_CHNL *) pvSetBuffer;

	kalMemCopy(&prAdapter->rAddRoamScnChnl,
		prRoamScnChnl,
		*pu4SetInfoLen);

	DBGLOG(INIT, TRACE,
		"set roam scan channel num is %d\n",
		prRoamScnChnl->ucChannelListNum);

	/* update cached channel list */
	aisFsmGetCurrentEssChnlList(prAdapter,
	GET_IOCTL_BSSIDX(prAdapter));

	return WLAN_STATUS_SUCCESS;
}

void aisFsmNotifyManageChannelList(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct AIS_SPECIFIC_BSS_INFO *ais;
	struct PARAM_MANAGE_CHANNEL_LIST *list;
	struct CONNECTION_SETTINGS *conn;
	uint8_t i = 0;
	uint8_t essChnlNum = 0;
	uint32_t size = 0;

	wiphy = prAdapter->prGlueInfo->prDevHandler->ieee80211_ptr->wiphy;
	wdev = wlanGetNetDev(prAdapter->prGlueInfo, ucBssIndex)->ieee80211_ptr;
	ais = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	conn = aisGetConnSettings(prAdapter, ucBssIndex);

	essChnlNum = ais->ucCurEssChnlInfoNum;
	size = sizeof(struct PARAM_MANAGE_CHANNEL_LIST) +
		essChnlNum * sizeof(uint32_t);
	list = kalMemAlloc(size, VIR_MEM_TYPE);
	if (!list) {
		DBGLOG(AIS, ERROR, "alloc mgmt chnl list event fail\n");
		return;
	}

	kalMemZero(list, size);
	list->id = GRID_MANAGE_FREQ_LIST;
	list->len = size - 2;
	list->num = essChnlNum;
	COPY_SSID(list->ssid, i, conn->aucSSID, conn->ucSSIDLen);

	for (i = 0; i < essChnlNum; i++)
		list->frequencies[i] = KHZ_TO_MHZ(nicChannelNum2Freq(
			ais->arCurEssChnlInfo[i].ucChannel,
			ais->arCurEssChnlInfo[i].eBand));

	mtk_cfg80211_vendor_event_generic_response(
		wiphy, wdev, size, (uint8_t *)list);
	kalMemFree(list, VIR_MEM_TYPE, size);
}

void aisFsmRunEventNchoActionFrameTx(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prAisBssInfo = (struct BSS_INFO *)NULL;
	struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg =
	    (struct MSG_MGMT_TX_REQUEST *)NULL;
	struct MSDU_INFO *prMgmtFrame = (struct MSDU_INFO *)NULL;
	struct _ACTION_VENDOR_SPEC_FRAME_T *prVendorSpec = NULL;
	uint8_t *pucFrameBuf = (uint8_t *) NULL;
	struct NCHO_INFO *prNchoInfo = NULL;
	uint16_t u2PktLen = 0;
	uint8_t ucBssIndex = 0;

	do {
		prMgmtTxMsg = (struct MSG_MGMT_TX_REQUEST *)prMsgHdr;
		if(prMgmtTxMsg == NULL)
			break;
		ucBssIndex = prMgmtTxMsg->ucBssIdx;

		prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);
		prNchoInfo = &(prAdapter->rNchoInfo);
		prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);

		if (prAisFsmInfo == NULL)
			break;

		u2PktLen =
		    (uint16_t) OFFSET_OF(struct _ACTION_VENDOR_SPEC_FRAME_T,
					 aucElemInfo[0]) +
		    prNchoInfo->rParamActionFrame.i4len + MAC_TX_RESERVED_FIELD;
		prMgmtFrame = cnmMgtPktAlloc(prAdapter, u2PktLen);
		if (prMgmtFrame == NULL) {
			DBGLOG(REQ, ERROR,
			       "NCHO there is no memory for prMgmtFrame\n");
			break;
		}
		prMgmtTxMsg->prMgmtMsduInfo = prMgmtFrame;

		pucFrameBuf =
		    (uint8_t *) ((uintptr_t)prMgmtFrame->prPacket +
				 MAC_TX_RESERVED_FIELD);
		prVendorSpec =
		    (struct _ACTION_VENDOR_SPEC_FRAME_T *)pucFrameBuf;
		prVendorSpec->u2FrameCtrl = MAC_FRAME_ACTION;
		prVendorSpec->u2Duration = 0;
		prVendorSpec->u2SeqCtrl = 0;
		COPY_MAC_ADDR(prVendorSpec->aucDestAddr,
			      prNchoInfo->rParamActionFrame.aucBssid);
		COPY_MAC_ADDR(prVendorSpec->aucSrcAddr,
			      prAisBssInfo->aucOwnMacAddr);
		COPY_MAC_ADDR(prVendorSpec->aucBSSID, prAisBssInfo->aucBSSID);

		kalMemCopy(prVendorSpec->aucElemInfo,
			   prNchoInfo->rParamActionFrame.aucData,
			   prNchoInfo->rParamActionFrame.i4len);

		prMgmtFrame->u2FrameLength = u2PktLen;

		aisFuncTxMgmtFrame(prAdapter,
				   &prAisFsmInfo->rMgmtTxInfo,
				   prMgmtTxMsg->prMgmtMsduInfo,
				   prMgmtTxMsg->u8Cookie,
				   ucBssIndex);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}

#endif

#if CFG_EXT_ROAMING_WTC
void wlanWtcModeInit(struct ADAPTER *prAdapter)
{
	kalMemZero(&prAdapter->rWtcModeInfo, sizeof(struct WTC_MODE_INFO));

	prAdapter->rWtcModeInfo.ucWtcMode = 2;
}

void aisWtcNeedPartialScan(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t *puDoPartialScan)
{
	struct ROAMING_INFO *roam = NULL;
	struct BSS_TRANSITION_MGT_PARAM *prBtmReq;
	struct WTC_MODE_INFO *prWtc;

	roam = aisGetRoamingInfo(prAdapter, ucBssIndex);
	prBtmReq = aisGetBTMParam(prAdapter, ucBssIndex);
	prWtc = &prAdapter->rWtcModeInfo;

	if (roam->eReason == ROAMING_REASON_BTM && prBtmReq->ucIsCisco) {
		if (prWtc->ucScanMode == 1)
			*puDoPartialScan = TRUE;
		else if (prWtc->ucScanMode > 1)
			*puDoPartialScan = FALSE;
	}
}

uint8_t wnmWtcGetRejectStatus(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	uint8_t ucStatus = WNM_BSS_TM_REJECT_NO_SUITABLE_CANDIDATES;
	struct BSS_TRANSITION_MGT_PARAM *btm;

	btm =  aisGetBTMParam(prAdapter, ucBssIndex);

	if (btm->ucIsCisco) {
		btm->ucVsieReasonCode = 0x00;
		ucStatus = WNM_BSS_TM_REJECT_UNSPECIFIED;
	}

	return ucStatus;
}

u_int8_t scanWtcCheckBssDesc(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct BSS_DESC *prBssDesc)
{
	struct BSS_TRANSITION_MGT_PARAM *prBtmParam;
	struct WTC_MODE_INFO *prWtcInfo;

	prWtcInfo = &prAdapter->rWtcModeInfo;
	prBtmParam = aisGetBTMParam(prAdapter, ucBssIndex);

	if (prBtmParam->ucIsCisco) {
		int32_t candRssi = RCPI_TO_dBm(
			prBssDesc->ucRCPI);

		if (prBssDesc->eBand == BAND_2G4 &&
			candRssi <
			prWtcInfo->cRssiThreshold_24G)
			return FALSE;
		else if (prBssDesc->eBand == BAND_5G &&
			candRssi <
			prWtcInfo->cRssiThreshold_5G)
			return FALSE;
#if (CFG_SUPPORT_WIFI_6G == 1)
		else if (prBssDesc->eBand == BAND_6G &&
			candRssi <
			prWtcInfo->cRssiThreshold_6G)
			return FALSE;
#endif
	}

	return TRUE;
}

void aisWtcSearchHandleBssDesc(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct BSS_TRANSITION_MGT_PARAM *prBtmParam;

	prBtmParam = aisGetBTMParam(prAdapter, ucBssIndex);
	if (prBtmParam)
		prBtmParam->ucVsieReasonCode = 0x05;
}

void wnmWtcRecvBtmReq(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct BSS_TRANSITION_MGT_PARAM *prBtmParam;

	prBtmParam = aisGetBTMParam(prAdapter, ucBssIndex);
	if (prBtmParam)
		prBtmParam->ucIsCisco = FALSE;
}

void wnmWtcCheckDiconnect(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct BSS_TRANSITION_MGT_PARAM *prBtmParam;
	struct BSS_INFO *prBssInfo;

	prBtmParam = aisGetBTMParam(prAdapter, ucBssIndex);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,ucBssIndex);

	/* Check if we nned to disconnect under WTC mode*/
	if (prBtmParam->ucIsCisco && prBtmParam->ucVsieReasonCode == 0x00) {
		DBGLOG(WNM, WARN, "BTM: WTC disconnect directly\n");
		/* TODO: the disassociation frame reason code must be set to 0X0C */
		prBssInfo->u2DeauthReason = REASON_CODE_DISASSOC_LEAVING_BSS;
		aisFsmStateAbort(prAdapter,
			DISCONNECT_REASON_CODE_LOCALLY,
			FALSE, prBssInfo->ucBssIndex);
	}
}

uint8_t wnmWtcCheckRejectAp(
	struct ADAPTER *adapter,
	uint8_t *ie,
	uint16_t len,
	uint8_t bssIndex)
{
	struct BSS_TRANSITION_MGT_PARAM *btm;
	struct WTC_MODE_INFO *wtc;
	const uint8_t *pos;
	int8_t rssi;

	pos = kalFindIeMatchMask(ELEM_ID_VENDOR,
		ie, len, "\x00\x40\x96\x2b",
		4, 2, NULL);

	/* Not cisco AP */
	if (!pos)
		return FALSE;

	DBGLOG(WNM, INFO, "WNM: Is Cisco AP!\n");

#if CFG_SUPPORT_REPORT_LOG
	wnmBTMReqReportLog(adapter,
		bssIndex,
		*(pos + 7),
		*(pos + 8));
#endif

	wtc = &adapter->rWtcModeInfo;
	rssi = adapter->rLinkQuality.rLq[bssIndex].cRssi;
	btm = aisGetBTMParam(adapter, bssIndex);
	btm->ucIsCisco = TRUE;

	/* WTC disable */
	if (wtc->ucWtcMode) {
		btm->ucVsieReasonCode = wtc->ucWtcMode;
		DBGLOG(WNM, INFO,
			"WNM: WTC mode disable\n");
		return TRUE;
	}

	/* AP suspend */
	if (adapter->prGlueInfo->fgIsInSuspendMode) {
		btm->ucVsieReasonCode = 0x06;
		DBGLOG(WNM, INFO,
			"WNM: AP is in suspendMode\n");
		return TRUE;
	}

	/* Rssi is good enough */
	if (rssi > wtc->cRssiThreshold) {
		btm->ucVsieReasonCode = 0x07;
		DBGLOG(WNM, INFO,
			"WNM: Rssi is good enough\n");
		return TRUE;
	}

	/* Scan mode disabled */
	if (wtc->ucScanMode == 0) {
		btm->ucVsieReasonCode = 0x00;
		DBGLOG(WNM, INFO,
			"WNM: Scan mode disabled\n");
		return TRUE;
	}

	return FALSE;
}

#endif

void roamingFsmSetSingleScanCadence(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamingInfo = NULL;
	struct ROAMING_SCAN_CADENCE *prScanCadence = NULL;

	prRoamingInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);
	if (!prRoamingInfo)
		return;
	prScanCadence = &prRoamingInfo->rScanCadence;

	switch (prRoamingInfo->eReason) {
	case ROAMING_REASON_POOR_RCPI:
	{
		/* <1> Adjust rssi threshold by FW, driver do nothing */
		/* <2> Configure a scan timer #1 */
		prScanCadence->ucScanSource = ROAMING_SCAN_SINGLE_TIMER;
		prScanCadence->u4ScanScheduleSec = SEC_TO_MSEC(10);
		break;
	}
	case ROAMING_REASON_HIGH_CU:
	{
		/* <1> Adjust CU condition by FW, driver do nothing */
		/* <2> Configure a scan timer #1 */
		prScanCadence->ucScanSource = ROAMING_SCAN_SINGLE_TIMER;
		prScanCadence->u4ScanScheduleSec = SEC_TO_MSEC(10);
		break;
	}
	case ROAMING_REASON_INACTIVE_TIMER:
	case ROAMING_REASON_SCAN_TIMER:
	{
		if (prScanCadence->ucFullScanCount == 0) {
			/* <2> Configure a scan timer #2 */
			prScanCadence->ucScanSource =
				ROAMING_SCAN_INACTIVE_TIMER;
			prScanCadence->u4ScanScheduleSec = SEC_TO_MSEC(10);
		} else {
			/* <2> Configure a periodic timer */
			prScanCadence->ucScanSource =
				ROAMING_SCAN_PERIODIC_TIMER;
			prScanCadence->u4ScanScheduleSec = SEC_TO_MSEC(120);
		}
		break;
	}
	case ROAMING_REASON_IDLE:
	{
		DBGLOG(ROAMING, INFO, "No target found, ending idle roaming\n");
		return;
	}
	default:
		return;
	}
	DBGLOG(ROAMING, INFO,
		"Start roaming timer, reason[%u] source[%u] time[%u]\n",
		prRoamingInfo->eReason, prScanCadence->ucScanSource,
		prScanCadence->u4ScanScheduleSec);
	/* <3> Start scan timer */
	if (timerPendingTimer(&prScanCadence->rScanTimer))
		cnmTimerStopTimer(prAdapter, &prScanCadence->rScanTimer);

	cnmTimerStartTimer(prAdapter, &prScanCadence->rScanTimer,
			   prScanCadence->u4ScanScheduleSec);
}

void roamingFsmRunScanTimerTimeout(struct ADAPTER *prAdapter,
				  uintptr_t ulParamPtr)
{
	struct BSS_DESC *prBssDesc = NULL;
	struct ROAMING_INFO *prRoamingFsmInfo = NULL;
	struct CMD_ROAMING_TRANSIT rRoamingData = {0};
	struct ROAMING_SCAN_CADENCE *prScanCadence = NULL;
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;

	prBssDesc = aisGetTargetBssDesc(prAdapter, ucBssIndex);
	prRoamingFsmInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);
	prScanCadence = &prRoamingFsmInfo->rScanCadence;
	kalMemZero(&rRoamingData, sizeof(struct CMD_ROAMING_TRANSIT));

	if (!roamingFsmIsDiscovering(prAdapter, ucBssIndex)) {
		DBGLOG(ROAMING, INFO, "AIS isn't in connected state\n");
		return;
	}

	/* update cached channel list */
	aisFsmGetCurrentEssChnlList(prAdapter, ucBssIndex);

	prRoamingFsmInfo->ucRcpi = prBssDesc->ucRCPI;
	rRoamingData.eReason =
		prScanCadence->ucScanSource == ROAMING_SCAN_INACTIVE_TIMER ?
		ROAMING_REASON_INACTIVE_TIMER : ROAMING_REASON_SCAN_TIMER;
	rRoamingData.u2Data = prBssDesc->ucRCPI;
	rRoamingData.u2RcpiLowThreshold = prRoamingFsmInfo->ucThreshold;
	rRoamingData.ucBssidx = ucBssIndex;
	roamingFsmRunEventDiscovery(prAdapter, &rRoamingData);

}

uint8_t roamingFsmScheduleNextSearch(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct ROAMING_INFO *prRoamingInfo = NULL;
	enum ENUM_ROAMING_REASON eReason = ROAMING_REASON_POOR_RCPI;

	prRoamingInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);
	if (!prRoamingInfo)
		return TRUE;
	eReason = prRoamingInfo->eReason;

	if (eReason == ROAMING_REASON_POOR_RCPI ||
		eReason == ROAMING_REASON_HIGH_CU ||
		eReason == ROAMING_REASON_INACTIVE_TIMER ||
		eReason == ROAMING_REASON_SCAN_TIMER ||
		eReason == ROAMING_REASON_IDLE)
		return FALSE;

	if (prRoamingInfo->rScanCadence.ucScanSource == ROAMING_SCAN_FORCE_FULL)
		return FALSE;

	return TRUE;
}

void aisRoamNeedPartialScan(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t *doPartialScan)
{
	struct AIS_FSM_INFO *ais = NULL;
	struct BSS_INFO *bss = NULL;
	struct ROAMING_INFO *roam = NULL;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct BSS_TRANSITION_MGT_PARAM *prBtmReq;
	struct WTC_MODE_INFO *prWtc;
	uint8_t discovering = FALSE;
	uint8_t postponing = FALSE;

	ais = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	bss = aisGetAisBssInfo(prAdapter, ucBssIndex);
	roam = aisGetRoamingInfo(prAdapter, ucBssIndex);
	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	discovering = bss->eConnectionState == MEDIA_STATE_CONNECTED &&
		     (roam->eCurrentState == ROAMING_STATE_DISCOVERY ||
		      roam->eCurrentState == ROAMING_STATE_ROAM);
	postponing = aisFsmIsInProcessPostpone(prAdapter, ucBssIndex);
	prBtmReq = aisGetBTMParam(prAdapter, ucBssIndex);
	prWtc = &prAdapter->rWtcModeInfo;
	*doPartialScan = TRUE;

	if (!discovering && !postponing)
		*doPartialScan = FALSE;

	/* Always do a partial scan for BTO */
	if (postponing)
		*doPartialScan = TRUE;

#if (CFG_EXT_ROAMING == 1)
	if (roam->eReason == ROAMING_REASON_BTM && prBtmReq->ucIsCisco) {
		if (prWtc->ucScanMode == 1)
			*doPartialScan = TRUE;
		else if (prWtc->ucScanMode > 1)
			*doPartialScan = FALSE;
	} else if (roam->eReason == ROAMING_REASON_IDLE) {
		*doPartialScan = FALSE;
	} else if (roam->rScanCadence.ucScanSource == ROAMING_SCAN_INACTIVE_TIMER ||
		roam->rScanCadence.ucScanSource == ROAMING_SCAN_FORCE_FULL) {
		*doPartialScan = FALSE;
	}
#if CFG_SUPPORT_LLW_SCAN
	/* If CRT DATA is 2, prohibit full roaming scan
	 * even if fgTargetChnlScanIssued is FALSE.
	 */
	if (ais->ucLatencyCrtDataMode == 2)
		*doPartialScan = TRUE;
#endif
#endif
	DBGLOG(AIS, INFO, "Discovering[%u] Postpone[%u] PartialScan[%u]\n",
		discovering, postponing, *doPartialScan);
}

void roamingDumpConfig(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
  	/* Dump configurations */
#define TEMP_LOG_TEMPLATE \
	"[Roam][Common]MinRoamDelta:%u Delta:%u [Idle]Delta:%u " \
	"[BeaconLost/Emergency]MinRssi:%d [BTM]Delta:%u " \
	"[WTC]ScanMode:%u RssiTH:%d 2GRssiTH:%d 5GRssiTH:%d 6GRssiTH:%d "\
	"[RBTC]ScoreWeight:%u ETPWeight:%u MinRssi:%d Delta:%u "\
	"[AP Scoring]RssiWeight:%u CUWeight:%u " \
	"Band1-Rssi-Factor-Val-Score(1/2/3/4/5): "\
			"%d/%d/%d/%d/%d-%u/%u/%u/%u/%u " \
	"Band2-Rssi-Factor-Val-Score(1/2/3/4/5): "\
			"%d/%d/%d/%d/%d-%u/%u/%u/%u/%u " \
	"Band3-Rssi-Factor-Val-Score(1/2/3/4): "\
			"%d/%d/%d/%d-%u/%u/%u/%u " \
	"Band1-CU-Factor-Val-Score(1/2):%u/%u-%u/%u " \
	"Band2-CU-Factor-Val-Score(1/2):%u/%u-%u/%u " \
	"Band3-CU-Factor-Val-Score(1/2):%u/%u-%u/%u " \

	DBGLOG(ROAMING, EVENT, TEMP_LOG_TEMPLATE,
		   prWifiVar->ucRCMinRoamDetla, prWifiVar->ucRCDelta,
		   prWifiVar->ucRIDelta, prWifiVar->cRBMinRssi,
		   prWifiVar->ucRBTMDelta, prWifiVar->ucScanMode,
		   prWifiVar->cRssiThreshold, prWifiVar->cRssiThreshold_24G,
		   prWifiVar->cRssiThreshold_5G, prWifiVar->cRssiThreshold_6G,
		   prWifiVar->ucRBTCScoreW, prWifiVar->ucRBTCETPW,
		   prWifiVar->cRBTCRssi, prWifiVar->ucRBTCDelta,
		   prWifiVar->ucRssiWeight, prWifiVar->ucCUWeight,
		   prWifiVar->cB1RssiFactorVal1, prWifiVar->cB1RssiFactorVal2,
		   prWifiVar->cB1RssiFactorVal3, prWifiVar->cB1RssiFactorVal4,
		   prWifiVar->cB1RssiFactorVal5,
		   prWifiVar->ucB1RssiFactorScore1,
		   prWifiVar->ucB1RssiFactorScore2,
		   prWifiVar->ucB1RssiFactorScore3,
		   prWifiVar->ucB1RssiFactorScore4,
		   prWifiVar->ucB1RssiFactorScore5,
		   prWifiVar->cB2RssiFactorVal1, prWifiVar->cB2RssiFactorVal2,
		   prWifiVar->cB2RssiFactorVal3, prWifiVar->cB2RssiFactorVal4,
		   prWifiVar->cB2RssiFactorVal5,
		   prWifiVar->ucB2RssiFactorScore1,
		   prWifiVar->ucB2RssiFactorScore2,
		   prWifiVar->ucB2RssiFactorScore3,
		   prWifiVar->ucB2RssiFactorScore4,
		   prWifiVar->ucB2RssiFactorScore5,
		   prWifiVar->cB3RssiFactorVal1, prWifiVar->cB3RssiFactorVal2,
		   prWifiVar->cB3RssiFactorVal3, prWifiVar->cB3RssiFactorVal4,
		   prWifiVar->ucB3RssiFactorScore1,
		   prWifiVar->ucB3RssiFactorScore2,
		   prWifiVar->ucB3RssiFactorScore3,
		   prWifiVar->ucB3RssiFactorScore4,
		   prWifiVar->ucB1CUFactorVal1, prWifiVar->ucB1CUFactorVal2,
		   prWifiVar->ucB1CUFactorScore1,
		   prWifiVar->ucB1CUFactorScore2,
		   prWifiVar->ucB2CUFactorVal1, prWifiVar->ucB2CUFactorVal2,
		   prWifiVar->ucB2CUFactorScore1,
		   prWifiVar->ucB2CUFactorScore2,
		   prWifiVar->ucB3CUFactorVal1, prWifiVar->ucB3CUFactorVal2,
		   prWifiVar->ucB3CUFactorScore1,
		   prWifiVar->ucB3CUFactorScore2);
#undef TEMP_LOG_TEMPLATE
}
