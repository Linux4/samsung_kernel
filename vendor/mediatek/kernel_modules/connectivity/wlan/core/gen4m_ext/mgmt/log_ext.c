/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /mgmt/log_ext.c
 */

/*! \file   "log_ext.c"
 *  \brief This file includes log report support.
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
#if CFG_ENABLE_WIFI_DIRECT
#include "gl_p2p_os.h"
#endif
#if (CFG_SUPPORT_CONN_LOG == 1)
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include <net/cfg80211.h>
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define RPTMACSTR	"%pM"
#define RPTMAC2STR(a)	a

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
enum ENUM_ROAMING_CATAGORY {
	ROAMING_CATEGORY_UNSPECIFIC = 0,
	ROAMING_CATEGORY_LOW_RSSI = 1,
	ROAMING_CATEGORY_HIGH_CU = 2,
	ROAMING_CATEGORY_BEACON_LOST = 3,
	ROAMING_CATEGORY_EMERGENCY = 4,
	ROAMING_CATEGORY_BTM_REQ = 5,
	ROAMING_CATEGORY_IDLE = 6,
	ROAMING_CATEGORY_WTC = 7,
	ROAMING_CATEGORY_INACTIVE_TIMER = 8,
	ROAMING_CATEGORY_SCAN_TIMER = 9,
	ROAMING_CATEGORY_BTCOEX = 10,
};

static uint8_t apucRoamingReasonToLog[ROAMING_REASON_NUM] = {
	ROAMING_CATEGORY_LOW_RSSI,		 /* map to ROAMING_REASON_POOR_RCPI(0) */
	ROAMING_CATEGORY_UNSPECIFIC,	 /* map to ROAMING_REASON_TX_ERR(1) */
	ROAMING_CATEGORY_UNSPECIFIC,	 /* map to ROAMING_REASON_RETRY(2) */
	ROAMING_CATEGORY_IDLE,			 /* map to ROAMING_REASON_IDLE(3) */
	ROAMING_CATEGORY_HIGH_CU,		 /* map to ROAMING_REASON_HIGH_CU(4)*/
	ROAMING_CATEGORY_BTCOEX,		 /* map to ROAMING_REASON_BT_COEX(5) */
	ROAMING_CATEGORY_BEACON_LOST,	 /* map to ROAMING_REASON_BEACON_TIMEOUT(6) */
	ROAMING_CATEGORY_UNSPECIFIC,	 /* map to ROAMING_REASON_INACTIVE(7) */
	ROAMING_CATEGORY_EMERGENCY, 	 /* map to ROAMING_REASON_SAA_FAIL(8) */
	ROAMING_CATEGORY_UNSPECIFIC,	 /* map to ROAMING_REASON_UPPER_LAYER_TRIGGER(9) */
	ROAMING_CATEGORY_BTM_REQ,		 /* map to ROAMING_REASON_BTM(10) */
	ROAMING_CATEGORY_INACTIVE_TIMER, /* map to ROAMING_REASON_INACTIVE_TIMER(11) */
	ROAMING_CATEGORY_SCAN_TIMER,	 /* map to ROAMING_REASON_SCAN_TIMER(12) */
};

static char *tx_status_text(uint8_t rTxDoneStatus)
{
	switch (rTxDoneStatus) {
	case TX_RESULT_SUCCESS: return "ACK";
	case TX_RESULT_MPDU_ERROR: return "NO_ACK";
	default: return "TX_FAIL";
	}
}

/*
 * EAP Method Types as allocated by IANA:
 * http://www.iana.org/assignments/eap-numbers
 */
enum eap_type {
	EAP_TYPE_NONE = 0,
	EAP_TYPE_IDENTITY = 1 /* RFC 3748 */,
	EAP_TYPE_NOTIFICATION = 2 /* RFC 3748 */,
	EAP_TYPE_NAK = 3 /* Response only, RFC 3748 */,
	EAP_TYPE_MD5 = 4, /* RFC 3748 */
	EAP_TYPE_OTP = 5 /* RFC 3748 */,
	EAP_TYPE_GTC = 6, /* RFC 3748 */
	EAP_TYPE_TLS = 13 /* RFC 2716 */,
	EAP_TYPE_LEAP = 17 /* Cisco proprietary */,
	EAP_TYPE_SIM = 18 /* RFC 4186 */,
	EAP_TYPE_TTLS = 21 /* RFC 5281 */,
	EAP_TYPE_AKA = 23 /* RFC 4187 */,
	EAP_TYPE_PEAP = 25 /* draft-josefsson-pppext-eap-tls-eap-06.txt */,
	EAP_TYPE_MSCHAPV2 = 26 /* draft-kamath-pppext-eap-mschapv2-00.txt */,
	EAP_TYPE_TLV = 33 /* draft-josefsson-pppext-eap-tls-eap-07.txt */,
	EAP_TYPE_TNC = 38 /* TNC IF-T v1.0-r3; note: tentative assignment;
			   * type 38 has previously been allocated for
			   * EAP-HTTP Digest, (funk.com) */,
	EAP_TYPE_FAST = 43 /* RFC 4851 */,
	EAP_TYPE_PAX = 46 /* RFC 4746 */,
	EAP_TYPE_PSK = 47 /* RFC 4764 */,
	EAP_TYPE_SAKE = 48 /* RFC 4763 */,
	EAP_TYPE_IKEV2 = 49 /* RFC 5106 */,
	EAP_TYPE_AKA_PRIME = 50 /* RFC 5448 */,
	EAP_TYPE_GPSK = 51 /* RFC 5433 */,
	EAP_TYPE_PWD = 52 /* RFC 5931 */,
	EAP_TYPE_EKE = 53 /* RFC 6124 */,
	EAP_TYPE_TEAP = 55 /* RFC 7170 */,
	EAP_TYPE_EXPANDED = 254 /* RFC 3748 */
};

enum ENUM_EAP_CODE {
	ENUM_EAP_CODE_NONE,
	ENUM_EAP_CODE_REQ,
	ENUM_EAP_CODE_RESP,
	ENUM_EAP_CODE_SUCCESS,
	ENUM_EAP_CODE_FAIL,

	ENUM_EAP_CODE_NUM
};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#ifndef WLAN_AKM_SUITE_FILS_SHA256
#define WLAN_AKM_SUITE_FILS_SHA256	0x000FAC0E
#endif
#ifndef WLAN_AKM_SUITE_FILS_SHA384
#define WLAN_AKM_SUITE_FILS_SHA384	0x000FAC0F
#endif
#ifndef WLAN_AKM_SUITE_FT_FILS_SHA256
#define WLAN_AKM_SUITE_FT_FILS_SHA256	0x000FAC10
#endif
#ifndef WLAN_AKM_SUITE_FT_FILS_SHA384
#define WLAN_AKM_SUITE_FT_FILS_SHA384	0x000FAC11
#endif
#ifndef WLAN_GET_SEQ_SEQ
#define WLAN_GET_SEQ_SEQ(seq) \
	(((seq) & (~(BIT(3) | BIT(2) | BIT(1) | BIT(0)))) >> 4)
#endif
#ifndef WPA_KEY_INFO_KEY_TYPE
#define WPA_KEY_INFO_KEY_TYPE BIT(3) /* 1 = PairwSd7ise, 0 = Group key */
#endif
/* bit4..5 is used in WPA, but is reserved in IEEE 802.11i/RSN */
#ifndef WPA_KEY_INFO_KEY_INDEX_MASK
#define WPA_KEY_INFO_KEY_INDEX_MASK (BIT(4) | BIT(5))
#endif
#ifndef WPA_KEY_INFO_MIC
#define WPA_KEY_INFO_MIC BIT(8)
#endif
#ifndef WPA_KEY_INFO_ENCR_KEY_DATA
#define WPA_KEY_INFO_ENCR_KEY_DATA BIT(12) /* IEEE 802.11i/RSN only */
#endif
#ifndef AES_BLOCK_SIZE
#define AES_BLOCK_SIZE 16
#endif
#ifndef ieee802_1x_hdr_size
/* struct ieee802_1x_hdr in wpa_supplicant */
#define ieee802_1x_hdr_size 4
#endif
#ifndef wpa_eapol_key_key_info_offset
/* struct wpa_eapol_key in wpa_supplicant */
#define wpa_eapol_key_key_info_offset 1
#endif
#ifndef wpa_eapol_key_fixed_field_size
#define wpa_eapol_key_fixed_field_size 77
#endif

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
struct PARAM_CONNECTIVITY_LOG {
	uint8_t id;
	uint8_t len;
	uint8_t msg[0];
};

void kalReportWifiLog(struct ADAPTER *prAdapter, uint8_t ucBssIndex,
				uint8_t *log)
{
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct PARAM_CONNECTIVITY_LOG *log_info;
	uint32_t size = sizeof(struct PARAM_CONNECTIVITY_LOG);
	struct timespec64 rNowTs;
	char log_time[300] = {0};
	uint32_t log_len = 0;

	wiphy = wlanGetWiphy();
	if (!wiphy)
		return;

	if (!wlanGetNetDev(prAdapter->prGlueInfo,
		ucBssIndex))
		return;
	wdev = wlanGetNetDev(prAdapter->prGlueInfo,
		ucBssIndex)->ieee80211_ptr;

	/*
	 * if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucLogEnhancement))
	 *	return;
	 */

	ktime_get_ts64(&rNowTs);
	kalSnprintf(log_time, sizeof(log_time),
		"[%ld.%06u]%s",
		rNowTs.tv_sec, rNowTs.tv_nsec, log);
	log_len = strlen(log_time) + 1;
	size += log_len;

	log_info = kalMemAlloc(size, VIR_MEM_TYPE);
	if (!log_info) {
		DBGLOG(AIS, ERROR,
			"alloc mgmt chnl list event fail\n");
		return;
	}

	kalMemZero(log_info, size);
	log_info->id = GRID_SWPIS_CONNECTIVITY_LOG;

	kalMemCopy(log_info->msg, log_time,
		strlen(log_time));
	log_info->len = size - 2;

	DBGLOG(AIS, INFO, "size(%d): %s\n",
		log_info->len, log_info->msg);

	mtk_cfg80211_vendor_event_generic_response(
		wiphy, wdev, size, (uint8_t *)log_info);

	kalMemFree(log_info, VIR_MEM_TYPE, size);
}

#if CFG_SUPPORT_WPA3_LOG
void wpa3LogAuthTimeout(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec)
{
#if (CFG_EXT_VERSION > 1)
	struct BSS_INFO *prBssInfo;

	prBssInfo = aisGetAisBssInfo(prAdapter,
		prStaRec->ucBssIndex);

	if (prBssInfo &&
		rsnKeyMgmtSae(prBssInfo->u4RsnSelectedAKMSuite) &&
		prStaRec->ucAuthAlgNum ==
		AUTH_ALGORITHM_NUM_OPEN_SYSTEM) {
		DBGLOG(SAA, INFO, "WPA3 auth open no response!");
		prStaRec->u2StatusCode = WPA3_AUTH_OPEN_NO_RESP;
	}
#else
	struct CONNECTION_SETTINGS *prConnSettings;

	prConnSettings = aisGetConnSettings(prAdapter,
		prStaRec->ucBssIndex);

	if (prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[0] ==
		WLAN_AKM_SUITE_SAE &&
		prStaRec->ucAuthAlgNum ==
		AUTH_ALGORITHM_NUM_OPEN_SYSTEM) {
		DBGLOG(SAA, INFO, "WPA3 auth open no response!");
		prStaRec->u2StatusCode = WPA3_AUTH_OPEN_NO_RESP;
	}
#endif
}

void wpa3LogAssocTimeout(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec)
{
	struct BSS_INFO *prBssInfo;

	prBssInfo = aisGetAisBssInfo(prAdapter,
		prStaRec->ucBssIndex);

	if (prBssInfo &&
		prBssInfo->u4RsnSelectedAKMSuite
		== WLAN_AKM_SUITE_SAE) {
		DBGLOG(SAA, INFO, "WPA3 assoc no response!");
		prStaRec->u2StatusCode = WPA3_ASSOC_NO_RESP;
	}
}

void wpa3LogConnect(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct CONNECTION_SETTINGS *prConnSettings;

	prConnSettings = aisGetConnSettings(
		prAdapter,
		ucBssIndex);

	if (prConnSettings)
		prConnSettings->u2JoinStatus =
			WLAN_STATUS_AUTH_TIMEOUT;
}

void wpa3Log6gPolicyFail(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	enum ENUM_PARAM_AUTH_MODE eAuthMode)
{
	struct BSS_INFO *prBssInfo;

	prBssInfo = aisGetAisBssInfo(prAdapter,
		ucBssIndex);

	if (prBssInfo &&
		(eAuthMode == AUTH_MODE_WPA3_SAE))
		aisGetConnSettings(prAdapter,
		prBssInfo->ucBssIndex)
		->u2JoinStatus = WPA3_6E_REJECT_NO_H2E;
}

void wpa3LogJoinFail(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo)
{
	struct CONNECTION_SETTINGS *prConnSettings;
	prConnSettings = aisGetConnSettings(prAdapter,
		prBssInfo->ucBssIndex);

	if (prBssInfo->u4RsnSelectedAKMSuite ==
		WLAN_AKM_SUITE_SAE) {
		DBGLOG(AIS, INFO,
			"WPA3 auth no response!");
		prConnSettings->u2JoinStatus =
			WPA3_AUTH_SAE_NO_RESP;
	}
}

uint16_t wpa3LogJoinFailStatus(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo)
{
	uint16_t u2JoinStatus;

	if (prBssInfo->u4RsnSelectedAKMSuite
			== WLAN_AKM_SUITE_SAE) {
		DBGLOG(INIT, INFO, "WPA3: cannot find network");
		u2JoinStatus = WPA3_NO_NETWORK_FOUND;
	} else
		u2JoinStatus = WLAN_STATUS_AUTH_TIMEOUT;

	return u2JoinStatus;
}

void wpa3LogExternalAuth(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint16_t status)
{
	if ((status == WLAN_STATUS_SUCCESS) &&
		(prStaRec->eAuthAssocState == SAA_STATE_EXTERNAL_AUTH))
		prStaRec->u2StatusCode = STATUS_CODE_SUCCESSFUL;
}

void wpa3LogMgmtTx(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	uint8_t ucBssIndex = prMsduInfo->ucBssIndex;
	struct AIS_FSM_INFO *prAisFsmInfo =
		aisGetAisFsmInfo(prAdapter, ucBssIndex);
	struct CONNECTION_SETTINGS *prConnSettings;


	if (rTxDoneStatus == TX_RESULT_SUCCESS)
		return;

	/* Only report fail case */

	prConnSettings = aisGetConnSettings(prAdapter,
		ucBssIndex);

	if (ucBssIndex == NETWORK_TYPE_AIS
		&& prAisFsmInfo->ucAvailableAuthTypes
		== AUTH_TYPE_SAE) {
		DBGLOG(AIS, INFO,
			"WPA3 auth SAE mgmt TX fail!");
		if (rTxDoneStatus == TX_RESULT_MPDU_ERROR) {
			prConnSettings->u2JoinStatus
				= WPA3_AUTH_SAE_NO_ACK;
		} else {
			prConnSettings->u2JoinStatus
				= WPA3_AUTH_SAE_SENDING_FAIL;
		}
	}
}

void wpa3LogSaaTx(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;

	if (rTxDoneStatus == TX_RESULT_SUCCESS)
		return;

	/* Only report fail case */

	prStaRec = cnmGetStaRecByIndex(
		prAdapter,
		prMsduInfo->ucStaRecIndex);
	if (!prStaRec)
		return;

	prBssInfo = aisGetAisBssInfo(prAdapter,
		prStaRec->ucBssIndex);

	if (prBssInfo &&
		prBssInfo->u4RsnSelectedAKMSuite
		== WLAN_AKM_SUITE_SAE
		&& prStaRec->ucAuthAlgNum
		== AUTH_ALGORITHM_NUM_OPEN_SYSTEM) {
		DBGLOG(SAA, INFO,
			"WPA3 auth open TX fail!");
		if (rTxDoneStatus
			== TX_RESULT_MPDU_ERROR) {
			prStaRec->u2StatusCode
			= WPA3_AUTH_OPEN_NO_ACK;
		} else {
			prStaRec->u2StatusCode
			= WPA3_AUTH_OPEN_SENDING_FAIL;
		}
	}
}

void wpa3LogSaaStart(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec)
{
	if (prStaRec->ucAuthAlgNum ==
		AUTH_ALGORITHM_NUM_SAE)
		prStaRec->u2StatusCode =
			WPA3_AUTH_SAE_NO_RESP;
}
#endif

#if CFG_SUPPORT_REPORT_LOG
void wnmLogBTMRecvReq(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct ACTION_BTM_REQ_FRAME *prRxFrame,
	uint16_t u2NRLen)
{
	char log[256] = {0};
	uint16_t u2Offset = 0;
	/* uint16_t u2NRLen = prSwRfb->u2PacketLen - u2TmpLen; */
	uint8_t ucNRCount = 0;
	uint8_t *pucIe = &prRxFrame->aucOptInfo[0];

	IE_FOR_EACH(pucIe, u2NRLen, u2Offset) {
		if (IE_ID(pucIe) != ELEM_ID_NEIGHBOR_REPORT)
			continue;
		ucNRCount++;
	}
	kalSprintf(log,
		"[BTM] REQ token=%d mode=%d disassoc=%d validity=%d candidate_list_cnt=%d",
		prRxFrame->ucDialogToken, prRxFrame->ucRequestMode,
		prRxFrame->u2DisassocTimer,
		prRxFrame->ucValidityInterval,
		ucNRCount < WNM_MAX_NEIGHBOR_REPORT ?
			ucNRCount : WNM_MAX_NEIGHBOR_REPORT);
	kalReportWifiLog(prAdapter, ucBssIndex, log);
}

void wnmLogBTMReqCandiReport(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct NEIGHBOR_AP *prNeighborAP,
	uint32_t cnt)
{
	char log[256] = {0};

	if (prNeighborAP->fgFromBtm) {
		kalSprintf(log,
			"[BTM] REQ_CANDI[%d] bssid=" RPTMACSTR
			" preference=%d",
			cnt, RPTMAC2STR(prNeighborAP->aucBssid),
			prNeighborAP->ucPreference);
		cnt++;
		kalReportWifiLog(prAdapter, ucBssIndex, log);
	}
}

void wnmLogBTMRespReport(
	struct ADAPTER *adapter,
	struct MSDU_INFO *prMsduInfo,
	uint8_t dialogToken,
	uint8_t status,
	uint8_t reason,
	uint8_t delay,
	const uint8_t *bssid)
{
	char log[256] = {0};
	const uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;

	kalSprintf(log,
		"[BTM] RESP toke=%d status=%d delay=%d target=" RPTMACSTR,
		dialogToken, status, delay,
		RPTMAC2STR(bssid ? bssid : aucZeroMacAddr));

	kalReportWifiLog(adapter, prMsduInfo->ucBssIndex, log);

}

void wnmLogBTMQueryReport(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct ACTION_BTM_QUERY_FRAME *prTxFrame)
{
	char log[256] = {0};

	kalSprintf(log,
		"[BTM] QUERY token=%d reason=%d",
		prTxFrame->ucDialogToken, prTxFrame->ucQueryReason);
	kalReportWifiLog(prAdapter, prStaRec->ucBssIndex, log);
}

void wnmBTMReqReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucReasonCode,
	uint8_t ucSubCode)
{
	char aucLog[256] = {0};

	if (!prAdapter)
		return;

	kalSprintf(aucLog,
		"[BTM] WTC reason=%u sub_code=%u duration=0",
		ucReasonCode);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void wnmBTMRespReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucReasonCode)
{
	char aucLog[256] = {0};

	if (!prAdapter)
		return;

	kalSprintf(aucLog,
		"[BTM] WTC reason_code=%u",
		ucReasonCode);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void rrmReqNeighborReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucToken,
	uint8_t *pucSsid,
	uint8_t ucSSIDLen)
{
	char aucLog[256] = {0};
	uint8_t reqSsid[ELEM_MAX_LEN_SSID + 1] = {0};

	if (!prAdapter)
		return;

	kalMemCopy(reqSsid, pucSsid,
		kal_min_t(uint8_t, ucSSIDLen, ELEM_MAX_LEN_SSID));

	kalSprintf(aucLog,
		"[NBR_RPT] REQ token=%u ssid=\"%s\"",
		ucToken, reqSsid);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void rrmRespNeighborReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucToken)
{
#if CFG_SUPPORT_802_11K
	struct LINK *prNeighborAPLink;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct NEIGHBOR_AP *prNeiAP = NULL;
	char aucLog[256] = {0};
	char aucScanChannel[200] = {0};
	uint32_t channels[MAX_CHN_NUM];
	uint32_t num_channels = 0;
	uint32_t freq = 0;
	uint32_t j = 0;
	uint32_t i = 0;
	uint8_t ucIdx = 0, ucPos = 0, ucAvailableLen = 195, ucMaxLen = 195;

	if (!prAdapter)
		return;

	prAisSpecBssInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	if (!prAisSpecBssInfo) {
		log_dbg(SCN, INFO, "No prAisSpecBssInfo\n");
		return;
	}

	kalMemZero(channels, sizeof(channels));

	prNeighborAPLink = &prAisSpecBssInfo->rNeighborApList.rUsingLink;
	if (LINK_IS_EMPTY(prNeighborAPLink))
		return;

	LINK_FOR_EACH_ENTRY(prNeiAP, prNeighborAPLink,
		rLinkEntry, struct NEIGHBOR_AP) {
		freq = nicChannelNum2Freq(
			prNeiAP->ucChannel,
			prNeiAP->eBand) / 1000;

		if (freq) {
			u_int8_t fgFound = FALSE;

			for (i = 0 ; i < j; i++) {
				if (freq == channels[i]) {
					fgFound = TRUE;
					break;
				}
			}

			if (!fgFound)
				channels[j++] = freq;
		}
	}

	num_channels = j;

	for (ucIdx = 0; (ucIdx <
		num_channels &&
		ucAvailableLen > 0); ucIdx++) {
		ucPos +=
			kalSnprintf(aucScanChannel + ucPos,
			ucMaxLen - ucPos,
			"%u ", channels[ucIdx]);
		ucAvailableLen =
			(ucMaxLen > ucPos) ? (ucMaxLen - ucPos) : 0;
		if (!ucAvailableLen)
			aucScanChannel[199] = '\0';
	}

	kalSprintf(aucLog,
		"[NBR_RPT] RESP token=%u freq[%d]=%s report_number=%d",
		ucToken,
		num_channels,
		aucScanChannel,
		prNeighborAPLink->u4NumElem);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
#endif
}

static char *rrmRequestModeToText(uint8_t ucMode)
{
	switch (ucMode) {
	case RM_BCN_REQ_PASSIVE_MODE: return "passive";
	case RM_BCN_REQ_ACTIVE_MODE: return "active";
	case RM_BCN_REQ_TABLE_MODE: return "beacon_table";
	default: return "";
	}
}

void rrmReqBeaconReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucToken,
	uint8_t ucRequestMode,
	uint8_t ucOpClass,
	uint8_t ucChannelNum,
	uint32_t u4Duration,
	uint32_t u4Mode)
{
	char aucLog[256] = {0};

	if (!prAdapter)
		return;

	kalSprintf(aucLog,
		"[BCN_RPT] REQ token=%u mode=%s operating_class=%u channel=%u duration=%u request_mode=0x%02x",
		ucToken,
		rrmRequestModeToText(ucRequestMode),
		ucOpClass,
		ucChannelNum,
		u4Duration,
		u4Mode);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void rrmRespBeaconReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucToken,
	uint32_t u4ReportNum)
{
	char aucLog[256] = {0};

	if (!prAdapter)
		return;

	kalSprintf(aucLog,
		"[BCN_RPT] RESP token=%u report_number=%u",
		ucToken,
		u4ReportNum);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}
#endif

#if CFG_SUPPORT_SCAN_LOG
void scanUpdateBcn(
	struct ADAPTER *prAdapter,
	struct BSS_DESC *prBss,
	uint8_t ucBssIndex)
{
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct SCAN_INFO *prScanInfo;
	struct PARAM_BCN_INFO *bcnInfo;
	uint8_t size = sizeof(struct PARAM_BCN_INFO);
	uint8_t i;

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	if (prScanInfo && !prScanInfo->fgBcnReport) {
		DBGLOG(AIS, TRACE,
			"[SWIPS] beacon report not started\n");
		return;
	}

	if (ucBssIndex >= KAL_AIS_NUM) {
		DBGLOG(AIS, ERROR,
			"[SWIPS] Dont update within other BSS\n");
		return;
	}

	wiphy = wlanGetWiphy();
	wdev = wlanGetNetDev(
		prAdapter->prGlueInfo, ucBssIndex)->ieee80211_ptr;

	bcnInfo = kalMemAlloc(size, VIR_MEM_TYPE);
	if (!bcnInfo) {
		DBGLOG(AIS, ERROR,
			"[SWIPS] beacon report event alloc fail\n");
		return;
	}

	kalMemZero(bcnInfo, size);
	bcnInfo->id = GRID_SWPIS_BCN_INFO;
	bcnInfo->len = size - 2;
	COPY_SSID(bcnInfo->ssid, i,
		  prBss->aucSSID, prBss->ucSSIDLen);
	COPY_MAC_ADDR(bcnInfo->bssid, prBss->aucBSSID);
	bcnInfo->channel = prBss->ucChannelNum;
	bcnInfo->bcnInterval = prBss->u2BeaconInterval;
	bcnInfo->timeStamp[0] = prBss->u8TimeStamp.u.LowPart;
	bcnInfo->timeStamp[1] = prBss->u8TimeStamp.u.HighPart;
	bcnInfo->sysTime = prBss->rUpdateTime;

	DBGLOG(INIT, INFO, "[SWIPS] Send[%d:%d] %s:%d (" MACSTR
	       ") with ch:%d interval:%d tsf:%u %u sys:%lu\n",
	       bcnInfo->id, bcnInfo->len, bcnInfo->ssid, i,
	       bcnInfo->bssid, bcnInfo->channel, bcnInfo->bcnInterval,
	       bcnInfo->timeStamp[0], bcnInfo->timeStamp[1], bcnInfo->sysTime);
	DBGLOG_MEM8(INIT, TRACE, bcnInfo, size);

	mtk_cfg80211_vendor_event_generic_response(
		wiphy, wdev, size, (uint8_t *)bcnInfo);

	kalMemFree(bcnInfo, VIR_MEM_TYPE, size);
}

void scanAbortBeaconRecv(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	enum ABORT_REASON abortReason)
{
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct PARAM_BCN_INFO_ABORT *bcnAbort;
	uint32_t size = sizeof(struct PARAM_BCN_INFO_ABORT);
	struct SCAN_INFO *prScanInfo =
		&(prAdapter->rWifiVar.rScanInfo);
	struct CMD_SET_REPORT_BEACON_STRUCT beaconRecv = {0};

	if (prScanInfo->fgBcnReport == FALSE) {
		DBGLOG(AIS, ERROR,
			"[SWIPS] Dont abort, SWIPS already OFF\n");
		return;
	}

	if (ucBssIndex >= KAL_AIS_NUM) {
		DBGLOG(AIS, ERROR,
			"[SWIPS] Dont abort within other BSS\n");
		return;
	}

	wiphy = wlanGetWiphy();
	wdev = wlanGetNetDev(prAdapter->prGlueInfo,
		ucBssIndex)->ieee80211_ptr;

	bcnAbort = kalMemAlloc(size, VIR_MEM_TYPE);
	if (!bcnAbort) {
		DBGLOG(AIS, ERROR,
			"[SWIPS] beacon report abort event alloc fail\n");
		return;
	}

	/* Send abort event to supplicant */
	bcnAbort->id = GRID_SWPIS_BCN_INFO_ABORT;
	bcnAbort->len = size - 2;
	bcnAbort->abort = abortReason;

	DBGLOG(INIT, INFO,
		"[SWIPS] Abort beacon report with reason: %d\n",
		bcnAbort->abort);

	mtk_cfg80211_vendor_event_generic_response(
		wiphy, wdev, size, (uint8_t *)bcnAbort);
	kalMemFree(bcnAbort, VIR_MEM_TYPE, size);

	/* Send stop SWIPS to FW */
	prScanInfo->fgBcnReport = FALSE;
	beaconRecv.ucReportBcnEn = FALSE;

	wlanSendSetQueryCmd(prAdapter,
		CMD_ID_SET_REPORT_BEACON,
		TRUE,
		FALSE,
		FALSE,
		nicCmdEventSetCommon,
		nicOidCmdTimeoutCommon,
		sizeof(struct CMD_SET_REPORT_BEACON_STRUCT),
		(uint8_t *) &beaconRecv, NULL, 0);
}

uint32_t
wlanoidSetBeaconRecv(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	struct CMD_SET_REPORT_BEACON_STRUCT beaconRecv = {0};
	struct SCAN_INFO *prScanInfo =
		&(prAdapter->rWifiVar.rScanInfo);

	beaconRecv.ucReportBcnEn = *((uint8_t *) pvSetBuffer);
	if (beaconRecv.ucReportBcnEn == prScanInfo->fgBcnReport) {
		DBGLOG(INIT, INFO, "[SWIPS] No need to %s again\n",
		       (beaconRecv.ucReportBcnEn == TRUE) ? "START" : "STOP");
		return WLAN_STATUS_SUCCESS;
	}

	DBGLOG(INIT, INFO, "[SWIPS] Set: %d\n", beaconRecv.ucReportBcnEn);

	prScanInfo->fgBcnReport = beaconRecv.ucReportBcnEn;

	return wlanSendSetQueryCmd(prAdapter,
		CMD_ID_SET_REPORT_BEACON,
		TRUE,
		FALSE,
		TRUE,
		nicCmdEventSetCommon,
		nicOidCmdTimeoutCommon,
		sizeof(struct CMD_SET_REPORT_BEACON_STRUCT),
		(uint8_t *) &beaconRecv, NULL, 0);
}

#endif

#if (CFG_SUPPORT_ROAMING_LOG == 1)
void roamingFsmLogScanStart(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex, uint8_t fgIsFullScn,
	struct BSS_DESC *prBssDesc)
{
	struct AIS_FSM_INFO *prAisFsmInfo;

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	if (roamingFsmIsDiscovering(prAdapter, ucBssIndex) &&
		(prAisFsmInfo->eCurrentState ==
		AIS_STATE_LOOKING_FOR)) {
		struct ROAMING_INFO *prRoamInfo;
		uint32_t u4CannelUtilization = 0;
		uint8_t ucIsValidCu = FALSE;
		char aucLog[256] = {0};

		prRoamInfo = aisGetRoamingInfo(prAdapter, ucBssIndex);
		ucIsValidCu = (prBssDesc && prBssDesc->fgExistBssLoadIE);
		if (ucIsValidCu)
			u4CannelUtilization =
				(prBssDesc->ucChnlUtilization * 100 / 255);

		if (fgIsFullScn)
			prRoamInfo->rScanCadence.ucFullScanCount++;

		kalSprintf(aucLog,
			"[ROAM] SCAN_START reason=%d rssi=%d cu=%d full_scan=%d full_scan_count=%u rssi_thres=%d",
			apucRoamingReasonToLog[prRoamInfo->eReason],
			RCPI_TO_dBm(prRoamInfo->ucRcpi),
			ucIsValidCu ? u4CannelUtilization : -1,
			fgIsFullScn, prRoamInfo->rScanCadence.ucFullScanCount,
			RCPI_TO_dBm(prRoamInfo->ucThreshold));

		kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
	}
}

void roamingFsmLogScanDoneImpl(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint16_t u2ApNum)
{
	struct SCAN_INFO *prScanInfo;
	struct LINK *prCurEssLink;
	char aucLog[256] = {0};
	char aucScanChannel[200] = {0};
	uint8_t ucIdx = 0, ucPos = 0, ucAvailableLen = 195, ucMaxLen = 195;

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prCurEssLink = &(aisGetAisSpecBssInfo(
			prAdapter, ucBssIndex)->rCurEssLink);

	DBGLOG(ROAMING, INFO, "Start to log scan done(%d)",
			prScanInfo->ucSparseChannelArrayValidNum);

	for (ucIdx = 0; (ucIdx < prScanInfo->ucSparseChannelArrayValidNum &&
		ucAvailableLen > 0); ucIdx++) {
		ucPos += kalSnprintf(aucScanChannel + ucPos, ucMaxLen - ucPos,
			"%d ", KHZ_TO_MHZ(nicChannelNum2Freq(
				prScanInfo->aucChannelNum[ucIdx],
				prScanInfo->aeChannelBand[ucIdx])));
		ucAvailableLen = (ucMaxLen > ucPos) ? (ucMaxLen - ucPos) : 0;
		if (!ucAvailableLen)
			aucScanChannel[199] = '\0';
	}

	kalSprintf(aucLog,
		"[ROAM] SCAN_DONE btcoex=%d ap_count=%d freq[%d]=%s",
#if (CFG_SUPPORT_APS == 1)
		aisGetApsInfo(prAdapter, ucBssIndex)->fgIsGBandCoex,
#else
		0,
#endif
		u2ApNum,
		prScanInfo->ucSparseChannelArrayValidNum,
		aucScanChannel);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void roamingFsmLogScanDone(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint16_t u2ApNum)
{
	/* LOG: scan start */
	if (roamingFsmIsDiscovering(prAdapter, ucBssIndex)) {
		struct AIS_FSM_INFO *prAisFsmInfo;

		prAisFsmInfo = aisGetAisFsmInfo(prAdapter,
			ucBssIndex);
		if (prAisFsmInfo->eCurrentState ==
			AIS_STATE_LOOKING_FOR)
			roamingFsmLogScanDoneImpl(
				prAdapter,
				ucBssIndex,
				u2ApNum);
	}
}

void roamingFsmLogSocre(struct ADAPTER *prAdapter, uint8_t *prefix,
	uint8_t ucBssIndex, struct BSS_DESC *prBssDesc, uint32_t u4Score,
	uint32_t u4Tput)
{
	char aucLog[256] = {0};
	char aucTput[24] = {0};

	if (!prBssDesc)
		return;

	if (u4Tput)
		kalSprintf(aucTput, " tp=%dkbps", u4Tput / 1000);

	kalSprintf(aucLog,
		"[ROAM] %s bssid=" RPTMACSTR
		" freq=%d rssi=%d cu=%d score=%d.%02d%s",
		prefix, RPTMAC2STR(prBssDesc->aucBSSID),
		KHZ_TO_MHZ(nicChannelNum2Freq(prBssDesc->ucChannelNum,
			prBssDesc->eBand)), RCPI_TO_dBm(prBssDesc->ucRCPI),
		(prBssDesc->fgExistBssLoadIE ?
			(prBssDesc->ucChnlUtilization * 100 / 255) : -1),
			u4Score / 100, u4Score % 100, aucTput);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void roamingFsmLogResultImpl(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct BSS_DESC *prSelectedBssDesc)
{
	char aucLog[256] = {0};
	uint8_t fgIsRoam =
		(prSelectedBssDesc && !prSelectedBssDesc->fgIsConnected);
	struct BSS_DESC *prBssDesc = (fgIsRoam ? prSelectedBssDesc :
		aisGetTargetBssDesc(prAdapter, ucBssIndex));

	kalSprintf(aucLog, "[ROAM] RESULT %s bssid=" RPTMACSTR,
		fgIsRoam ? "ROAM" : "NO_ROAM",
		RPTMAC2STR(prBssDesc->aucBSSID));

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void roamingFsmLogResult(
	struct ADAPTER *ad,
	uint8_t bidx,
	struct BSS_DESC *cand)
{
	if (roamingFsmIsDiscovering(ad, bidx))
		roamingFsmLogResultImpl(ad, bidx, cand);
}

void roamingFsmLogCancelImpl(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t *pucReason)
{
	char aucLog[256] = {0};

	kalSprintf(aucLog, "[ROAM] CANCELED [%s]", pucReason);

	kalReportWifiLog(prAdapter, ucBssIndex, aucLog);
}

void roamingFsmLogCancel(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	if (roamingFsmIsDiscovering(prAdapter, ucBssIndex))
		roamingFsmLogCancelImpl(prAdapter, ucBssIndex,
			"STA disconnected");
}

uint8_t roamingFsmIsDiscovering(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct BSS_INFO *prAisBssInfo = NULL;
	struct ROAMING_INFO *prRoamingFsmInfo = NULL;
	uint8_t fgIsDiscovering = FALSE;

	prAisBssInfo = aisGetAisBssInfo(prAdapter,
		ucBssIndex);
	prRoamingFsmInfo = aisGetRoamingInfo(prAdapter,
		ucBssIndex);

	fgIsDiscovering =
		prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED ||
		aisFsmIsInProcessPostpone(prAdapter, ucBssIndex);

	return fgIsDiscovering;
}
#endif

#if (CFG_SUPPORT_CONN_LOG == 1)
void kalBufferWifiLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t *log,
	uint8_t ucSn)
{
	struct BUFFERED_LOG_ENTRY *prLogEntry;

	/*
	 * if (IS_FEATURE_DISABLED(prAdapter->rWifiVar.ucLogEnhancement))
	 *	return;
	 */

	if (ucBssIndex >= MAX_BSSID_NUM)
		return;

	prLogEntry = &prAdapter->rWifiVar.rBufferedLog[ucBssIndex];
	kalMemZero(prLogEntry,
		sizeof(struct BUFFERED_LOG_ENTRY));

	prLogEntry->ucSn = ucSn;
	prLogEntry->fgBuffered = TRUE;

	kalMemCopy(prLogEntry->aucLog,
		log, sizeof(prLogEntry->aucLog));
}

static char *eap_type_text(uint8_t type)
{
	switch (type) {
	case EAP_TYPE_IDENTITY: return "Identity";
	case EAP_TYPE_NOTIFICATION: return "Notification";
	case EAP_TYPE_NAK: return "Nak";
	case EAP_TYPE_TLS: return "TLS";
	case EAP_TYPE_TTLS: return "TTLS";
	case EAP_TYPE_PEAP: return "PEAP";
	case EAP_TYPE_SIM: return "SIM";
	case EAP_TYPE_GTC: return "GTC";
	case EAP_TYPE_MD5: return "MD5";
	case EAP_TYPE_OTP: return "OTP";
	case EAP_TYPE_FAST: return "FAST";
	case EAP_TYPE_SAKE: return "SAKE";
	case EAP_TYPE_PSK: return "PSK";
	default: return "Unknown";
	}
}

static int wpa_mic_len(uint32_t akmp)
{
	switch (akmp) {
	case WLAN_AKM_SUITE_8021X_SUITE_B_192:
		return 24;
	case WLAN_AKM_SUITE_FILS_SHA256:
	case WLAN_AKM_SUITE_FILS_SHA384:
	case WLAN_AKM_SUITE_FT_FILS_SHA256:
	case WLAN_AKM_SUITE_FT_FILS_SHA384:
		return 0;
	default:
		return 16;
	}
}

void connLogEapKey(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t eventType,
	uint8_t *pucEapol,
	uint8_t ucSn)
{
	enum EVENT_TYPE {
		EVENT_RX,
		EVENT_TX,
	};
	char log[256] = {0};
	uint16_t u2KeyDataLen = 0;
	uint8_t mic_len = 16;
	uint8_t key_data_len_offset; /* fixed field len + mic len*/
	uint8_t isPairwise = FALSE;
	uint16_t u2KeyInfo = 0;
	uint8_t m = 0;
	uint32_t u4RsnSelectedAKMSuite = 0;
#if (CFG_EXT_VERSION == 1)
	struct CONNECTION_SETTINGS *prConnSettings;

	prConnSettings =
		aisGetConnSettings(prAdapter, ucBssIndex);
	if (prConnSettings) {
		mic_len = wpa_mic_len(
		prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[0]);
		u4RsnSelectedAKMSuite =
			prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[0];
	}
#else
	struct BSS_INFO *prBssInfo = NULL;

	prBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (prBssInfo) {
		mic_len = wpa_mic_len(
		prBssInfo->u4RsnSelectedAKMSuite);
		u4RsnSelectedAKMSuite =
			prBssInfo->u4RsnSelectedAKMSuite;
	}
#endif

	WLAN_GET_FIELD_BE16(&pucEapol[
		ieee802_1x_hdr_size
		+ wpa_eapol_key_key_info_offset],
		&u2KeyInfo);
	key_data_len_offset =
		ieee802_1x_hdr_size
		+ wpa_eapol_key_fixed_field_size
		+ mic_len;
	WLAN_GET_FIELD_BE16(&pucEapol[key_data_len_offset],
		&u2KeyDataLen);

#ifdef DBG_CONN_LOG_EAP
	DBGLOG(AIS, INFO,
		"akm=%x mic_len=%d key_data_len_offset=%d",
		u4RsnSelectedAKMSuite,
		mic_len, key_data_len_offset);
#endif

	switch (eventType) {
	case EVENT_RX:
		if (u2KeyInfo & WPA_KEY_INFO_KEY_TYPE) {
			if (u2KeyInfo
				& WPA_KEY_INFO_KEY_INDEX_MASK)
				DBGLOG(RX, WARN,
					"WPA: ignore EAPOL-key (pairwise) with non-zero key index");

			if (u2KeyInfo &
				(WPA_KEY_INFO_MIC
				| WPA_KEY_INFO_ENCR_KEY_DATA)) {
				m = 3;
				isPairwise = TRUE;
			} else {
				m = 1;
				isPairwise = TRUE;
			}
#ifdef DBG_CONN_LOG_EAP
			DBGLOG(RX, INFO,
				"<RX> EAPOL: key, M%d, KeyInfo 0x%04x KeyDataLen %d\n",
				m, u2KeyInfo, u2KeyDataLen);
#endif
		} else {
			if ((mic_len &&
					(u2KeyInfo
				& WPA_KEY_INFO_MIC)) ||
				(!mic_len &&
					(u2KeyInfo
				& WPA_KEY_INFO_ENCR_KEY_DATA)
				)) {
				m = 1;
				isPairwise = FALSE;
			} else {
				isPairwise = FALSE;
#ifdef DBG_CONN_LOG_EAP
				DBGLOG(RX, WARN,
					"WPA: EAPOL-Key (Group) without Mic/Encr bit");
#endif
			}
#ifdef DBG_CONN_LOG_EAP
			DBGLOG(RX, INFO,
				"<RX> EAPOL: group key, M%d, KeyInfo 0x%04x KeyDataLen %d\n",
				m, u2KeyInfo, u2KeyDataLen);
#endif
		}

		if (isPairwise)
			kalSprintf(log,
				"[EAPOL] 4WAY M%d", m);
		else
			kalSprintf(log,
				"[EAPOL] GTK M%d", m);
		kalReportWifiLog(prAdapter, ucBssIndex, log);

		break;
	case EVENT_TX:
		if (!(u2KeyInfo & WPA_KEY_INFO_KEY_TYPE)) {
			m = 2;
			isPairwise = FALSE;
#ifdef DBG_CONN_LOG_EAP
			DBGLOG(RX, INFO,
				"<TX> EAPOL: group key, M%d, KeyInfo 0x%04x KeyDataLen %d\n",
				m, u2KeyInfo, u2KeyDataLen);
#endif
		} else if (u2KeyDataLen == 0 ||
			(mic_len == 0 &&
			(u2KeyInfo
			& WPA_KEY_INFO_ENCR_KEY_DATA) &&
			u2KeyDataLen == AES_BLOCK_SIZE)) {
			m = 4;
			isPairwise = TRUE;
#ifdef DBG_CONN_LOG_EAP
			DBGLOG(RX, INFO,
				"<TX> EAPOL: key, M%d, KeyInfo 0x%04x KeyDataLen %d\n",
				m, u2KeyInfo, u2KeyDataLen);
#endif
		} else {
			m = 2;
			isPairwise = TRUE;
#ifdef DBG_CONN_LOG_EAP
			DBGLOG(RX, INFO,
				"<TX> EAPOL: key, M%d, KeyInfo 0x%04x KeyDataLen %d\n",
				m, u2KeyInfo, u2KeyDataLen);
#endif
		}

		if (isPairwise)
			kalSprintf(log,
				"[EAPOL] 4WAY M%d", m);
		else
			kalSprintf(log,
				"[EAPOL] GTK M%d", m);
		kalBufferWifiLog(prAdapter, ucBssIndex, log,
			ucSn);

		break;
	}
}

void connLogDhcpRx(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint32_t u4Opt)
{
	char log[256] = {0};

	switch (u4Opt & 0xffffff00) {
	case 0x35010100:
#if 0 /* Customer request */
		kalSprintf(log, "[DHCP] DISCOVERY");
		kalReportWifiLog(prAdapter,
			ucBssIndex, log);
#endif
		break;
	case 0x35010200:
		kalSprintf(log, "[DHCP] OFFER");
		kalReportWifiLog(prAdapter,
			ucBssIndex, log);
		break;
	case 0x35010300:
#if 0 /* Customer request */
		kalSprintf(log, "[DHCP] REQUEST");
		kalReportWifiLog(prAdapter,
			ucBssIndex, log);
#endif
		break;
	case 0x35010500:
		kalSprintf(log, "[DHCP] ACK");
		kalReportWifiLog(prAdapter,
			ucBssIndex, log);
		break;
	case 0x35010600:
		kalSprintf(log, "[DHCP] NAK");
		kalReportWifiLog(prAdapter,
			ucBssIndex, log);
		break;
	}
}

void connLogDhcpTx(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint32_t u4Opt,
	uint8_t ucSn)
{
	char log[256] = {0};

	/* client */
	switch (u4Opt & 0xffffff00) {
	case 0x35010100:
		kalSprintf(log, "[DHCP] DISCOVERY");
		kalBufferWifiLog(prAdapter,
			ucBssIndex, log, ucSn);
		break;
	case 0x35010200:
		kalSprintf(log, "[DHCP] OFFER");
		kalBufferWifiLog(prAdapter,
			ucBssIndex, log, ucSn);
		break;
	case 0x35010300:
		kalSprintf(log, "[DHCP] REQUEST");
		kalBufferWifiLog(prAdapter,
			ucBssIndex, log, ucSn);
		break;
	case 0x35010500:
		kalSprintf(log, "[DHCP] ACK");
		kalBufferWifiLog(prAdapter,
			ucBssIndex, log, ucSn);
		break;
	case 0x35010600:
		kalSprintf(log, "[DHCP] NAK");
		kalBufferWifiLog(prAdapter,
			ucBssIndex, log, ucSn);
		break;
	}
}

void connLogEapTx(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint16_t u2EapLen,
	uint8_t ucEapType,
	uint8_t ucEapCode,
	uint8_t ucSn)
{
	char log[256] = {0};

	/* 1:REQ, 2:RESP, 3:SUCCESS, 4:FAIL, 5:INIT, 6:Finish */
	uint8_t *apucEapCode[ENUM_EAP_CODE_NUM] = {
		(uint8_t *) DISP_STRING("UNKNOWN"),
		(uint8_t *) DISP_STRING("REQ"),
		(uint8_t *) DISP_STRING("RESP"),
		(uint8_t *) DISP_STRING("SUCC"),
		(uint8_t *) DISP_STRING("FAIL")
	};

	if (ucEapCode == 1 || ucEapCode == 2) {
		if (kalStrnCmp(
			eap_type_text(ucEapType),
			"Unknown", 7) != 0) {
			kalSprintf(log,
				"[EAP] %s type=%s len=%d",
			apucEapCode[ucEapCode],
			eap_type_text(ucEapType),
			u2EapLen);
			kalBufferWifiLog(prAdapter,
				ucBssIndex, log,
				ucSn);
		}
	} else if (ucEapCode == 3 ||
				ucEapCode == 4) {
			kalSprintf(log,
				"[EAP] %s",
				apucEapCode[ucEapCode]);
			kalBufferWifiLog(prAdapter,
				ucBssIndex, log,
				ucSn);
	} else {
		//EapCode = 5 or 6, no need to log
	}
}

void connLogEapRx(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint16_t u2EapLen,
	uint8_t ucEapType,
	uint8_t ucEapCode)
{
	char log[256] = {0};

	/* 1:REQ, 2:RESP, 3:SUCCESS, 4:FAIL, 5:INIT, 6:Finish */
	uint8_t *apucEapCode[ENUM_EAP_CODE_NUM] = {
		(uint8_t *) DISP_STRING("UNKNOWN"),
		(uint8_t *) DISP_STRING("REQ"),
		(uint8_t *) DISP_STRING("RESP"),
		(uint8_t *) DISP_STRING("SUCC"),
		(uint8_t *) DISP_STRING("FAIL")
	};

	if (ucEapCode == 1 || ucEapCode == 2) {
		if (kalStrnCmp(
			eap_type_text(ucEapType),
			"Unknown", 7) != 0) {
			kalSprintf(log,
				"[EAP] %s type=%s len=%d",
			apucEapCode[ucEapCode],
			eap_type_text(ucEapType),
			u2EapLen);
			kalReportWifiLog(prAdapter,
				ucBssIndex, log);
		}
	} else if (ucEapCode == 3 ||
			ucEapCode == 4) {
		kalSprintf(log,
			"[EAP] %s",
			apucEapCode[ucEapCode]);
		kalReportWifiLog(prAdapter,
			ucBssIndex, log);
	} else {
		//EapCode = 5 or 6, no need to log
	}
}

void connLogPkt(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	/* report EAPOL & DHCP packet only */
	if (prMsduInfo->ucPktType == ENUM_PKT_1X ||
		prMsduInfo->ucPktType == ENUM_PKT_DHCP) {
		char log[256] = {0};
		char buf[64] = {0};
		struct BUFFERED_LOG_ENTRY *prLogEntry;

		/* Check if any buffered log */
		prLogEntry = &prAdapter->rWifiVar.rBufferedLog[
			prMsduInfo->ucBssIndex];
		if (prLogEntry && prLogEntry->fgBuffered &&
			prLogEntry->ucSn == prMsduInfo->ucTxSeqNum)
			kalSprintf(buf, "%s", prLogEntry->aucLog);
		else
			if (prMsduInfo->ucPktType == ENUM_PKT_1X)
				kalSprintf(buf, "[EAP/EAPOL]");
			else
				kalSprintf(buf, "%s",
					TXS_PACKET_TYPE[prMsduInfo->ucPktType]);

		if (prMsduInfo->ucPktType == ENUM_PKT_1X)
			kalSprintf(log,
			"%s tx_status=%s sn=%d", buf,
				tx_status_text(rTxDoneStatus),
				prMsduInfo->u2HwSeqNum);
		else
			kalSprintf(log,
			"%s tx_status=%s sn=%d", buf,
				tx_status_text(rTxDoneStatus),
				prMsduInfo->u2HwSeqNum);

		kalReportWifiLog(prAdapter,
			prMsduInfo->ucBssIndex, log);
	}
}

void connLogConnect(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct cfg80211_connect_params *sme)
{
	char log[256] = {0};
	uint8_t aucSSID[ELEM_MAX_LEN_SSID+1];

	kalMemZero(aucSSID, ELEM_MAX_LEN_SSID+1);
	COPY_SSID(aucSSID, sme->ssid_len,
		sme->ssid, sme->ssid_len);

	kalSprintf(log, "[CONN] CONNECTING ssid=\"%s\"", aucSSID);
	if (sme->bssid)
		kalSprintf(log + strlen(log), " bssid=" RPTMACSTR,
			RPTMAC2STR(sme->bssid));
	if (sme->bssid_hint)
		kalSprintf(log + strlen(log), " bssid_hint=" RPTMACSTR,
			RPTMAC2STR(sme->bssid_hint));
	if (sme->channel && sme->channel->center_freq)
		kalSprintf(log + strlen(log), " freq=%d",
			sme->channel->center_freq);
	if (sme->channel_hint && sme->channel_hint->center_freq)
		kalSprintf(log + strlen(log), " freq_hint=%d",
			sme->channel_hint->center_freq);

	kalSprintf(log + strlen(log),
		" pairwise=0x%x group=0x%x akm=0x%x auth_type=%x btcoex=%d",
		sme->crypto.ciphers_pairwise[0],
		sme->crypto.cipher_group,
		sme->crypto.akm_suites[0],
		sme->auth_type,
		(aisGetAisBssInfo(prAdapter, ucBssIndex)->eCoexMode == COEX_TDD_MODE));
	kalReportWifiLog(prAdapter, ucBssIndex, log);

}

void connLogDisconnect(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	struct BSS_DESC *prTargetBssDesc = NULL;
	struct BSS_INFO *prAisBssInfo = NULL;
	struct AIS_FSM_INFO *prAisFsmInfo = NULL;

	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	prTargetBssDesc = aisGetTargetBssDesc(prAdapter,
		prAisBssInfo->ucBssIndex);
	if (prTargetBssDesc && prAisBssInfo->eConnectionState
		== MEDIA_STATE_DISCONNECTED &&
		prAisFsmInfo->ucReasonOfDisconnect ==
		DISCONNECT_REASON_CODE_RADIO_LOST) {
		char log[256] = {0};

		kalSprintf(log,
			"[CONN] DISCONN bssid=" RPTMACSTR
			" rssi=%d reason=%d",
			RPTMAC2STR(prTargetBssDesc->aucBSSID),
			RCPI_TO_dBm(prTargetBssDesc->ucRCPI), 0);
		kalReportWifiLog(prAdapter,
			prAisBssInfo->ucBssIndex, log);
	}
}

void connLogMgmtTx(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct AIS_FSM_INFO *prAisFsmInfo;
	uint8_t ucBssIndex = 0;
	char log[256] = {0};
	struct STA_RECORD *prStaRec;
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint16_t u2RxTransactionSeqNum;
	uint16_t u2TxAuthAlgNum;
	struct BSS_DESC *prTargetBssDesc;
	uint16_t u2AuthStatusCode;

	prStaRec =
		cnmGetStaRecByIndex(prAdapter,
			prMsduInfo->ucStaRecIndex);
	prTargetBssDesc =
		aisGetTargetBssDesc(prAdapter,
			ucBssIndex);
	prAisFsmInfo =
		aisGetAisFsmInfo(prAdapter,
			ucBssIndex);

	if (ucBssIndex == NETWORK_TYPE_AIS
		&& prAisFsmInfo->ucAvailableAuthTypes
		== AUTH_TYPE_SAE
		&& prTargetBssDesc
		&& prStaRec) {

		prAuthFrame = (struct WLAN_AUTH_FRAME *)
			(prMsduInfo->prPacket);
		u2TxAuthAlgNum = prAuthFrame->u2AuthAlgNum;

		if (u2TxAuthAlgNum == AUTH_ALGORITHM_NUM_SAE) {
			u2RxTransactionSeqNum =
				prAuthFrame->u2AuthTransSeqNo;
			u2AuthStatusCode =
				prAuthFrame->u2StatusCode;
			kalSprintf(log,
			"[CONN] AUTH REQ bssid=" RPTMACSTR
			" rssi=%d auth_algo=%d type=%d sn=%d status=%d tx_status=%s",
			RPTMAC2STR(prStaRec->aucMacAddr),
			RCPI_TO_dBm(
			prTargetBssDesc->ucRCPI),
			prStaRec->ucAuthAlgNum,
			u2RxTransactionSeqNum,
			prMsduInfo->ucTxSeqNum,
			u2AuthStatusCode,
			tx_status_text(rTxDoneStatus));
			kalReportWifiLog(prAdapter,
				prMsduInfo->ucBssIndex, log);
		}
	}
}

void connLogAssocResp(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct WLAN_ASSOC_RSP_FRAME *prAssocRspFrame,
	uint16_t u2RxStatusCode)
{
	if (IS_STA_IN_AIS(prStaRec) &&
		prStaRec->eAuthAssocState == SAA_STATE_WAIT_ASSOC2) {
		char log[256] = {0};
		uint16_t u2RxAssocId;

		u2RxAssocId = prAssocRspFrame->u2AssocId;
		if ((u2RxAssocId & BIT(6)) && (u2RxAssocId & BIT(7))
			&& !(u2RxAssocId & BITS(8, 15))) {
			u2RxAssocId = u2RxAssocId & ~BITS(6, 7);
		} else {
			u2RxAssocId = u2RxAssocId & ~AID_MSB;
		}
		if (prStaRec->fgIsReAssoc)
			kalSprintf(log, "[CONN] REASSOC");
		else
			kalSprintf(log, "[CONN] ASSOC");

		kalSprintf(log + strlen(log),
			" RESP bssid=" RPTMACSTR
			" sn=%d status=%d assoc_id=%d",
			RPTMAC2STR(prStaRec->aucMacAddr),
			WLAN_GET_SEQ_SEQ(prAssocRspFrame->u2SeqCtrl),
			u2RxStatusCode, u2RxAssocId);
		kalReportWifiLog(prAdapter, prStaRec->ucBssIndex, log);
	}
}

void connLogAuthReq(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct STA_RECORD *prStaRec;
	char log[256] = {0};
	struct BSS_DESC *prBssDesc = NULL;

	prStaRec = cnmGetStaRecByIndex(prAdapter,
		prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return;

	prBssDesc = scanSearchBssDescByBssid(prAdapter,
		prStaRec->aucMacAddr);

	if (IS_STA_IN_AIS(prStaRec) && prBssDesc) {
		struct WLAN_AUTH_FRAME *prAuthFrame;
		uint16_t u2AuthStatusCode;

		prAuthFrame = (struct WLAN_AUTH_FRAME *)
			prMsduInfo->prPacket;
		u2AuthStatusCode = prAuthFrame->u2StatusCode;
		kalSprintf(log,
			"[CONN] AUTH REQ bssid=" RPTMACSTR
			" rssi=%d auth_algo=%d sn=%d status=%d tx_status=%s",
			RPTMAC2STR(prStaRec->aucMacAddr),
			RCPI_TO_dBm(prBssDesc->ucRCPI),
			prStaRec->ucAuthAlgNum,
			prMsduInfo->u2HwSeqNum,
			u2AuthStatusCode,
			tx_status_text(rTxDoneStatus));
		kalReportWifiLog(prAdapter,
			prMsduInfo->ucBssIndex, log);
	}
}

void connLogAssocReq(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct STA_RECORD *prStaRec;
	char log[256] = {0};
	struct BSS_DESC *prBssDesc = NULL;

	prStaRec = cnmGetStaRecByIndex(prAdapter,
		prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return;

	prBssDesc = scanSearchBssDescByBssid(prAdapter,
		prStaRec->aucMacAddr);

	if (IS_STA_IN_AIS(prStaRec) && prBssDesc) {
		if (prStaRec->fgIsReAssoc)
			kalSprintf(log, "[CONN] REASSOC");
		else
			kalSprintf(log, "[CONN] ASSOC");

		kalSprintf(log + strlen(log),
		" REQ bssid=" RPTMACSTR
		" rssi=%d sn=%d tx_status=%s",
			RPTMAC2STR(prStaRec->aucMacAddr),
			RCPI_TO_dBm(prBssDesc->ucRCPI),
			prMsduInfo->u2HwSeqNum,
			tx_status_text(rTxDoneStatus));

		kalReportWifiLog(prAdapter,
			prMsduInfo->ucBssIndex, log);
	}
}

void connLogAuthResp(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct WLAN_AUTH_FRAME *prAuthFrame,
	uint16_t u2RxStatusCode)
{
	if (IS_STA_IN_AIS(prStaRec)) {
		char log[256] = {0};

		if (prStaRec->eAuthAssocState == SAA_STATE_WAIT_AUTH2
		|| prStaRec->eAuthAssocState == SAA_STATE_WAIT_AUTH4
		|| prStaRec->eAuthAssocState == SAA_STATE_SEND_AUTH1
		|| prStaRec->eAuthAssocState == SAA_STATE_SEND_AUTH3) {
			kalSprintf(log,
				"[CONN] AUTH RESP bssid=" RPTMACSTR
				" auth_algo=%d sn=%d status=%d",
			RPTMAC2STR(prStaRec->aucMacAddr),
			prStaRec->ucAuthAlgNum,
			WLAN_GET_SEQ_SEQ(prAuthFrame->u2SeqCtrl),
			u2RxStatusCode);
			kalReportWifiLog(prAdapter, prStaRec->ucBssIndex, log);
		} else if (prStaRec->eAuthAssocState ==
			SAA_STATE_EXTERNAL_AUTH) {
			kalSprintf(log,
				"[CONN] AUTH RESP bssid=" RPTMACSTR
				" auth_algo=%d type=%d sn=%d status=%d",
			RPTMAC2STR(prStaRec->aucMacAddr),
			prStaRec->ucAuthAlgNum,
			prAuthFrame->u2AuthTransSeqNo,
			WLAN_GET_SEQ_SEQ(prAuthFrame->u2SeqCtrl),
			u2RxStatusCode);
			kalReportWifiLog(prAdapter, prStaRec->ucBssIndex, log);
		}
	}
}

void connLogDeauth(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTxSeqNum,
	uint16_t u2ReasonCode)
{
	struct BSS_DESC *prBssDesc = NULL;
	char log[256] = {0};

	if (prStaRec && IS_STA_IN_AIS(prStaRec)) {
		prBssDesc = scanSearchBssDescByBssid(prAdapter,
			prStaRec->aucMacAddr);

		if (prBssDesc) {
			kalSprintf(log,
				"[CONN] DEAUTH TX bssid=" RPTMACSTR
				" rssi=%d sn=%d reason=%d",
			RPTMAC2STR(prStaRec->aucMacAddr),
			RCPI_TO_dBm(prBssDesc->ucRCPI),
			ucTxSeqNum,
			u2ReasonCode);

			kalReportWifiLog(prAdapter,
				prStaRec->ucBssIndex, log);
		}
	}
}

void connLogRxDeauth(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct WLAN_DEAUTH_FRAME *prDeauthFrame,
	struct BSS_DESC *prBssDesc)
{
	char log[256] = {0};

	if (prBssDesc) {
		kalSprintf(log,
		"[CONN] DEAUTH RX bssid=" RPTMACSTR
		" rssi=%d sn=%d reason=%d",
		RPTMAC2STR(prDeauthFrame->aucSrcAddr),
		RCPI_TO_dBm(prBssDesc->ucRCPI),
		WLAN_GET_SEQ_SEQ(
			prDeauthFrame->u2SeqCtrl),
		prDeauthFrame->u2ReasonCode);
		kalReportWifiLog(prAdapter,
			prStaRec->ucBssIndex, log);
	}

}

void connLogRxDeassoc(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct WLAN_DISASSOC_FRAME *prDisassocFrame,
	struct BSS_DESC *prBssDesc)
{
	char log[256] = {0};

	if (prBssDesc) {
		kalSprintf(log,
		"[CONN] DISASSOC RX bssid=" MACSTR
		" rssi=%d sn=%d reason=%d",
		RPTMAC2STR(prDisassocFrame->aucSrcAddr),
		RCPI_TO_dBm(prBssDesc->ucRCPI),
		WLAN_GET_SEQ_SEQ(
			prDisassocFrame->u2SeqCtrl),
		prDisassocFrame->u2ReasonCode);

		kalReportWifiLog(prAdapter,
			prStaRec->ucBssIndex, log);
	}
}

#endif


