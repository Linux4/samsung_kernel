/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /include/roaming_ext.h
 */

/*! \file   "roaming_ext.h"
 *  \brief This file includes roaming ext support.
 *    Detail description.
 */


#ifndef _ROAMING_EXT_H
#define _ROAMING_EXT_H

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

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */
#if CFG_SUPPORT_ASSURANCE
__KAL_ATTRIB_PACKED_FRONT__
struct IE_ASSURANCE_ROAMING_REASON {
	uint8_t ucId;
	uint8_t ucLength;
	uint8_t aucOui[3];
	uint8_t ucOuiType;
	uint8_t ucSubType;
	uint8_t ucVersion;
	uint8_t ucSubTypeReason;
	uint8_t ucReason;
	uint8_t ucSubTypeRcpi;
	uint8_t ucRcpi;
	uint8_t ucSubTypeRcpiThreshold;
	uint8_t ucRcpiThreshold;
	uint8_t ucSubTypeCuThreshold;
	uint8_t ucCuThreshold;
} __KAL_ATTRIB_PACKED__;

__KAL_ATTRIB_PACKED_FRONT__
struct IE_ASSURANCE_BEACON_REPORT {
	uint8_t ucId;
	uint8_t ucLength;
	uint8_t aucOui[3];
	uint8_t ucOuiType;
	uint8_t ucSubType;
	uint8_t ucVersion;
	uint8_t ucLen;
	uint8_t ucReason;
} __KAL_ATTRIB_PACKED__;
#endif

/* SS IE*/
#define VENDOR_IE_SS_OUI                      0x0000F0

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */
#define NCHO_CMD_MAX_LENGTH	128

#define DISCONNECT_REASON_CODE_NCHO_DIFF_BAND     12

#if (CFG_EXT_ROAMING_WTC && CFG_SUPPORT_REPORT_LOG)
#define APPEND_WTC_IE(_prBtmParam, _pucOptInfo, _ad, sta, len) \
	if (_prBtmParam->ucIsCisco) { \
			*_pucOptInfo++ = ELEM_ID_VENDOR; \
			*_pucOptInfo++ = 7; \
			WLAN_SET_FIELD_BE24(_pucOptInfo, 0x0000f0); \
			_pucOptInfo += 3; \
			*_pucOptInfo++ = 0x22; \
			*_pucOptInfo++ = 0x07; \
			*_pucOptInfo++ = 0x01; \
			*_pucOptInfo++ = _prBtmParam->ucVsieReasonCode; \
			DBGLOG(WNM, INFO,  "Vsie Reason Code: %d\n", \
				_prBtmParam->ucVsieReasonCode); \
			wnmBTMRespReportLog(_ad, \
				sta->ucBssIndex, \
				_prBtmParam->ucVsieReasonCode); \
			len += 9; \
	}
#endif

/******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */
#if (CFG_EXT_ROAMING == 1)
uint8_t apsIsGoodRCPI(struct ADAPTER *ad,
	struct BSS_DESC *bss,
	enum ENUM_ROAMING_REASON eRoamReason,
	uint8_t bidx);

uint8_t apsIsBssQualify(struct ADAPTER *ad,
	struct BSS_DESC *bss,
	enum ENUM_ROAMING_REASON eRoamReason,
	uint32_t u4ConnectedApScore,
	uint32_t u4CandidateApScore,
	uint8_t bidx);

uint32_t apsCalculateApScore(
	struct ADAPTER *ad,
	struct BSS_DESC *bss,
	enum ENUM_ROAMING_REASON eRoamType,
	uint8_t bidx);
#endif

#if CFG_SUPPORT_NCHO
void wlanNchoInit(
	struct ADAPTER *prAdapter,
	uint8_t fgFwSync);

uint32_t wlanNchoSetFWEnable(
	struct ADAPTER *prAdapter,
	uint8_t fgEnable);

uint32_t wlanNchoSetFWScanPeriod(
	struct ADAPTER *prAdapter,
	uint32_t u4RoamScanPeriod);

uint32_t
wlanoidSetNchoRoamTrigger(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamTrigger(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoRoamDelta(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamDelta(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoRoamScnPeriod(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamScnPeriod(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoRoamScnChnl(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidAddNchoRoamScnChnl(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamScnChnl(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoRoamScnCtrl(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoRoamScnCtrl(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoScnChnlTime(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoScnChnlTime(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoScnHomeTime(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoScnHomeTime(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoScnHomeAwayTime(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoScnHomeAwayTime(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoScnNprobes(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoScnNprobes(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidGetNchoReassocInfo(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSendNchoActionFrameStart(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSendNchoActionFrameEnd(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetNchoWesMode(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoWesMode(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoBand(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoBand(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoDfsScnMode(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoDfsScnMode(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoEnable(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryNchoEnable(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryNchoRoamBand(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetNchoRoamBand(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidAddRoamScnChnl(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidQueryRoamScnChnl(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidQueryRoamTrigger(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetRoamTrigger(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

void aisFsmNotifyManageChannelList(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void aisFsmRunEventNchoActionFrameTx(
	struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr);
#endif

#if CFG_SUPPORT_ASSURANCE
void rrmComposeRmRejectRep(
	struct ADAPTER *prAdapter,
	struct RADIO_MEASUREMENT_REPORT_PARAMS *prRep,
	uint8_t ucToken,
	uint8_t ucMeasType,
	uint8_t ucRejectMode,
	uint8_t ucBssIndex);

uint32_t
wlanoidSetDisconnectIes(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidSetRoamingReasonEnable(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidGetRoamingReasonEnable(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t
wlanoidSetBrErrReasonEnable(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen);

uint32_t
wlanoidGetBrErrReasonEnable(
	struct ADAPTER *prAdapter,
	void *pvQueryBuffer,
	uint32_t u4QueryBufferLen,
	uint32_t *pu4QueryInfoLen);

uint32_t assocCalculateRoamReasonLen(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIdx,
	struct STA_RECORD *prStaRec);

void assocGenerateRoamReason(struct
	ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo);

void deauth_build_nonwfa_vend_ie(
	struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo);

void kalUpdateDeauthInfo(
	struct GLUE_INFO *prGlueInfo,
	uint8_t *pucFrameBody,
	uint32_t u4FrameBodyLen,
	uint8_t ucBssIndex);
#endif

#if CFG_EXT_ROAMING_WTC
void wlanWtcModeInit(struct ADAPTER *prAdapter);

uint8_t wnmWtcGetRejectStatus(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

u_int8_t scanWtcCheckBssDesc(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	struct BSS_DESC *prBssDesc);

void aisWtcSearchHandleBssDesc(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void wnmWtcRecvBtmReq(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

uint8_t wnmWtcCheckRejectAp(
	struct ADAPTER *adapter,
	uint8_t *ie,
	uint16_t len,
	uint8_t bssIndex);

void wnmWtcCheckDiconnect(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);
#endif
#endif /* _ROAMING_EXT_H */

uint32_t wlanSetEssBandBitmap(struct ADAPTER *prAdapter,
	uint8_t ucEssBandBitMap);

void roamingFsmSetSingleScanCadence(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void roamingFsmRunScanTimerTimeout(struct ADAPTER *prAdapter,
				  uintptr_t ulParamPtr);

uint8_t roamingFsmScheduleNextSearch(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void roamingFsmInitScanTimer(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void roamingDumpConfig(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex);

void aisRoamNeedPartialScan(
	struct ADAPTER *prAdapter,
	uint8_t ucBssIndex,
	uint8_t *doPartialScan);
