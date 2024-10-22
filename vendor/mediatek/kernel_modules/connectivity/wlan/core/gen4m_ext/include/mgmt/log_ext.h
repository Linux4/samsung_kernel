/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /include/log_ext.h
 */

/*! \file   "log_ext.h"
 *  \brief This file includes log report support.
 *    Detail description.
 */


#ifndef _LOG_EXT_H
#define _LOG_EXT_H

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
enum WPA3_STATUS_REPORT {
	WPA3_NO_NETWORK_FOUND = 1025,
	WPA3_AUTH_OPEN_NO_ACK = 1026,
	AUTH_NO_ACK = WPA3_AUTH_OPEN_NO_ACK,
	WPA3_AUTH_OPEN_NO_RESP = 1027,
	AUTH_NO_RESP = WPA3_AUTH_OPEN_NO_RESP,
	WPA3_AUTH_OPEN_SENDING_FAIL = 1028,
	AUTH_SENDING_FAIL = WPA3_AUTH_OPEN_SENDING_FAIL,
	WPA3_AUTH_SAE_NO_ACK,
	WPA3_AUTH_SAE_NO_RESP,
	WPA3_AUTH_SAE_SENDING_FAIL,
	WPA3_ASSOC_NO_ACK = 1032,
	ASSOC_NO_ACK = WPA3_ASSOC_NO_ACK,
	WPA3_ASSOC_NO_RESP = 1033,
	ASSOC_NO_RESP = WPA3_ASSOC_NO_RESP,
	WPA3_ASSOC_SENDING_FAIL = 1034,
	ASSOC_SENDING_FAIL = WPA3_ASSOC_SENDING_FAIL,
	WPA3_6E_REJECT_NO_H2E = 1035,
	WPA3_STATUS_REPORT_NUM
};

enum ENUM_CONN_FAIL_REASON {
	CONN_FAIL_UNKNOWN,
	CONN_FAIL_DISALLOWED_LIST,
	CONN_FAIL_FWK_BLACKLIST,
	CONN_FAIL_RSN_MISMATCH,
	CONN_FAIL_BLACLIST_LIMIT,
	CONN_FAIL_REASON_NUM
};

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */
struct PARAM_BCN_INFO {
	uint8_t id;
	uint8_t len;
	uint8_t ssid[ELEM_MAX_LEN_SSID + 1];
	uint8_t bssid[PARAM_MAC_ADDR_LEN];
	uint8_t channel;
	uint16_t bcnInterval;
	uint32_t timeStamp[2];
	uint64_t sysTime;
} __KAL_ATTRIB_PACKED__;

enum ABORT_REASON {
	ABORT_SCAN_STARTS = 1,
	ABORT_CONNECT_STARTS,
	ABORT_DISCONNECT,
};

struct PARAM_BCN_INFO_ABORT {
	uint8_t id;
	uint8_t len;
	uint8_t abort;
};

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */
#if CFG_SUPPORT_WPA3_LOG
void wpa3LogSaaTx(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus);

void wpa3LogExternalAuth(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint16_t status);

void wpa3LogMgmtTx(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus);

void wpa3LogJoinFail(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo);

uint16_t wpa3LogJoinFailStatus(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo);

void wpa3LogSaaStart(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec);

void wpa3LogAuthTimeout(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec);

void wpa3LogAssocTimeout(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec);

void wpa3Log6gPolicyFail(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	enum ENUM_PARAM_AUTH_MODE eAuthMode);

#endif

#if CFG_SUPPORT_REPORT_LOG
void rrmReqNeighborReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucToken,
	uint8_t *pucSsid,
	uint8_t ucSSIDLen);

void rrmRespNeighborReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucToken);

void rrmReqBeaconReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucToken,
	uint8_t ucRequestMode,
	uint8_t ucOpClass,
	uint8_t ucChannelNum,
	uint32_t u4Duration,
	uint32_t u4Mode);

void rrmRespBeaconReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucToken,
	uint32_t u4ReportNum);

void wnmBTMReqReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucReasonCode,
	uint8_t ucSubCode);

void wnmBTMRespReportLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t ucReasonCode);

void wnmLogBTMRecvReq(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct ACTION_BTM_REQ_FRAME *prRxFrame,
	uint16_t u2NRLen);

void wnmLogBTMReqCandiReport(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct NEIGHBOR_AP *prNeighborAP,
	uint32_t cnt);

void wnmLogBTMRespReport(
	struct ADAPTER *adapter,
	struct MSDU_INFO *prMsduInfo,
	uint8_t dialogToken,
	uint8_t status,
	uint8_t reason,
	uint8_t delay,
	const uint8_t *bssid);

void wnmLogBTMQueryReport(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct ACTION_BTM_QUERY_FRAME *prTxFrame);
#endif

#if CFG_SUPPORT_SCAN_LOG
uint32_t
wlanoidSetBeaconRecv(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

void scanUpdateBcn(
	struct ADAPTER *prAdapter,
	struct BSS_DESC *prBss,
	uint8_t ucBssIndex);

void scanAbortBeaconRecv(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	enum ABORT_REASON abortReason);
#endif

#if WLAN_INCLUDE_SYS
uint32_t rsnSetCountryCodefor6g(
	struct ADAPTER *prAdapter,
	uint8_t mode);
#endif

#if (CFG_SUPPORT_ROAMING_LOG == 1)
void roamingFsmLogScanStart(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t fgIsFullScn,
	struct BSS_DESC *prBssDesc);

void roamingFsmLogScanDone(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint16_t u2ApNum);

void roamingFsmLogScanBuffer(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void roamingFsmLogScanInit(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void roamingFsmLogCurr(
	struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc,
	enum ENUM_ROAMING_REASON eRoamReason,
	uint8_t ucBssIndex);

void roamingFsmLogCandi(
	struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc,
	enum ENUM_ROAMING_REASON eRoamReason,
	uint8_t ucBssIndex);

void roamingFsmLogSocre(
	struct ADAPTER *prAdapter,
	uint8_t *prefix,
	uint8_t ucBssIndex,
	struct BSS_DESC *prBssDesc,
	uint32_t u4Score,
	uint32_t u4Tput);

void roamingFsmLogResult(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct BSS_DESC *prSelectedBssDesc);

void roamingFsmLogCancel(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

uint8_t roamingFsmIsDiscovering(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);
#endif

#if (CFG_SUPPORT_CONN_LOG == 1)
void kalReportWifiLog(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t *log);

void __kalReportWifiLog(
	struct ADAPTER *prAdapter);

void gResetStaInfoPrinted(void);

void connLogEapKey(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t eventType,
	uint8_t *pucEapol,
	uint8_t ucSn);

void connLogEapTx(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint16_t u2EapLen,
	uint8_t ucEapType,
	uint8_t ucEapCode,
	uint8_t ucSn);

void connLogEapRx(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint16_t u2EapLen,
	uint8_t ucEapType,
	uint8_t ucEapCode);

void connLogDhcpRx(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint32_t u4Xid,
	uint32_t u4Opt);

void connLogDhcpTx(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint32_t u4Xid,
	uint32_t u4Opt,
	uint8_t ucSn);

void connLogPkt(
	struct ADAPTER *prAdapter,
	struct MSDU_TOKEN_ENTRY *prTokenEntry,
	uint32_t u4Stat);

void connLogStaInfo(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct BSS_DESC_SET *set);

void connLogConnect(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct cfg80211_connect_params *sme);

void connLogConnectFail(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	enum ENUM_CONN_FAIL_REASON reason);

void connLogDisconnect(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void connLogMgmtTx(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus);

void connLogAssocResp(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct WLAN_ASSOC_RSP_FRAME *prAssocRspFrame,
	uint16_t u2RxStatusCode);

void connLogAuthReq(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus);

void connLogAssocReq(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo,
	enum ENUM_TX_RESULT_CODE rTxDoneStatus);

void connLogAuthResp(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct WLAN_AUTH_FRAME *prAuthFrame,
	uint16_t u2RxStatusCode);

void connLogDeauth(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucTxSeqNum,
	uint16_t u2ReasonCode);

void connLogRxDeauth(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct WLAN_DEAUTH_FRAME *prDeauthFrame,
	struct BSS_DESC *prBssDesc);

void connLogRxDeauthBuffer(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct WLAN_DEAUTH_FRAME *prDeauthFrame,
	struct BSS_DESC *prBssDesc);

void connLogRxDeauthPrint(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void connLogRxDeassoc(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct WLAN_DISASSOC_FRAME *prDisassocFrame,
	struct BSS_DESC *prBssDesc);
#endif

#if (CFG_SUPPORT_MLD_LOG == 1) && (CFG_SUPPORT_802_11BE_MLO == 1)
#define MLD_LOG_SUCCESS 0
#define MLD_LOG_FAILURE 1

void mldLogSetup(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	uint8_t ucStatus);

void mldLogT2LMStatus(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec);

void mldLogT2LMReq(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct STA_RECORD *prStaRec,
	uint8_t ucToken,
	uint8_t ucCnt,
	uint8_t rTxDoneStatus);

void mldLogT2LMResp(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct STA_RECORD *prStaRec,
	uint8_t ucToken,
	uint8_t ucStatus);

void mldLogT2LMTeardown(
	struct ADAPTER *prAdapter,
	struct BSS_INFO *prBssInfo,
	struct STA_RECORD *prStaRec,
	uint8_t rTxDoneStatus);

void mldLogLink(
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct MLD_STA_RECORD *prMldStaRec,
	uint8_t ucLinkState,
	uint8_t reason);
#endif

#endif /* _LOG_EXT_H */
