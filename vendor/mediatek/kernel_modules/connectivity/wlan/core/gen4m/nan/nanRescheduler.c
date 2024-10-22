/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nanRescheduler.c
 */

/*! \file   "nanRescheduler.c"
 *  \brief  This file defines the procedures for
 *          handling extended 6g rescheduling
 *          requirement from STD+
 *
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
#include "gl_kal.h"
#include "gl_rst.h"
#if (CFG_EXT_VERSION == 1)
#include "gl_sys.h"
#endif
#include "wlan_lib.h"
#include "debug.h"
#include "wlan_oid.h"
#include <linux/rtc.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/net.h>

#include "nan_ext.h"
#include "nan_ext_ccm.h"
#include "nan_ext_pa.h"
#include "nan_ext_mdc.h"
#include "nan_ext_asc.h"
#include "nan_ext_amc.h"
#include "nan_ext_ascc.h"
#include "nan_ext_fr.h"
#include "nan_ext_adsdc.h"
#include "nan_ext_eht.h"
#include "nanRescheduler.h"

#if (CFG_SUPPORT_NAN_RESCHEDULE  == 1)
#define LOGSTR_INIT "[RESCHEDULE_TRACE] (INIT RESCHEDULER)\n"
#define LOGSTR_DEINIT "[RESCHEDULE_TRACE] (DEINIT RESCHEDULER)\n"
#define LOGSTR_TOKEN_INFO "---[RESCHEDULE_TRACE] LVL2: TOKEN(ucTokenID:%u) INFO :)\n"
#define LOGSTR_NDL_INFO \
"---[RESCHEDULE_TRACE] LVL2: ucNdlIndex#%u:NDL(MAC:"MACSTR")\n"
#define LOGSTR_LVL1_SKIP "->[RESCHEDULE_TRACE] LVL1:EVENT=%s:RESCHEDULE REQUESTED but NO NDL. SKIP\n"
#define LOGSTR_LVL1_CHECKING "->[RESCHEDULE_TRACE] LVL1:EVENT=%s:CHECKING RESCHEDULE NEEDED\n"
#define LOGSTR_LVL1_CHECK_PENDING "->[RESCHEDULE_TRACE] LVL1:EVENT=%s:CHECKING PENDING RESCHEDULE TOKEN\n"
#define LOGSTR_LVL1_TRIG "->[RESCHEDULE_TRACE] LVL1:EVENT=%s:EXTERNAL RESCHEDULE REQUEST\n"
#define LOGSTR_LVL1_NOTOKEN "<-[RESCHEDULE_TRACE] LVL1:NO PENDING RESCHEDULE TOKEN\n"
#define LOGSTR_LVL2_GENTOKEN "-->[RESCHEDULE_TRACE] LVL2:GENERATE TOKEN(ucTokenID:%u, source:%s)\n"
#define LOGSTR_LVL2_NONDL "<--[RESCHEDULE_TRACE] LVL2:NO NDL in TOKEN(ucTokenID:%u, source:%s)\n"
#define LOGSTR_LVL2_DONE "<--[RESCHEDULE_TRACE] LVL2:Reschedule for Token(%u)->event(%s) is ALL DONE\n"
#define LOGSTR_LVL2_NEXT "-->[RESCHEDULE_TRACE] LVL2:dequeue next TOKEN(ucTokenID:%u, source:%s)\n"
#define LOGSTR_LVL2_NOTOKEN "<--[RESCHEDULE_TRACE] LVL2:No more token. END\n"
#define LOGSTR_LVL2_END "---[RESCHEDULE_TRACE] LVL2: ---------------------------\n"
#define LOGSTR_LVL3_START \
"--->[RESCHEDULE_TRACE] LVL3:RESCHEDULE(MAC:"MACSTR") START!\n"
#define LOGSTR_LVL3_FAIL \
"<---[RESCHEDULE_TRACE] LVL3:RESCHEDULE(MAC:"MACSTR") is FAILED.\n"
#define LOGSTR_LVL3_NEXT \
"--->[RESCHEDULE_TRACE] LVL3:RESCHEDULE next NDL(MAC:"MACSTR") START!\n"
#define LOGSTR_LVL3_DONE \
"<---[RESCHEDULE_TRACE] LVL3:RESCHEDULE(MAC:"MACSTR") is DONE.\n"
#define LOGSTR_LVL1_NONEED "<-[RESCHEDULE_TRACE] LVL1:RESCHEDULE NOT NEEDED\n"
#define LOGSTR_ETC1 "[RESCHEDULE_TRACE] INFO: it is not NDL reschedule(req) from this module.\n"
#define LOGSTR_ETC2 "[RESCHEDULE_TRACE] INFO: peer requested reschedule.\n"

/*sync with enum RESCHEDULE_SOURCE */
const char *RESCHEDULE_SOURCE_dbgstr[] = {
	"RESCHEDULE_NULL",
	"AIS_CONNECTED",
	"AIS_DISCONNECTED",
	"NEW_NDL",
	"REMOVE_NDL"
};

static int GetActiveNDPNum(const struct _NAN_NDL_INSTANCE_T *prNDL);
static struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T *
GenReScheduleToken(
	struct ADAPTER *prAdapter,
	const uint8_t fgRescheduleInputNDL,
	const enum RESCHEDULE_SOURCE event,
	const struct _NAN_NDL_INSTANCE_T *prNDL);

static struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T*
getOngoing_RescheduleToken(struct ADAPTER *prAdapter);
static struct _NAN_RESCHED_NDL_INFO*
getOngoing_RescheduleNDL(struct ADAPTER *prAdapter);
static struct _NAN_RESCHED_NDL_INFO*
getNewState_RescheduleNDL(struct ADAPTER *prAdapter);
static struct _NAN_RESCHED_NDL_INFO*
getNewState_RescheduleNDL_reorder(struct ADAPTER *prAdapter);
static void
FreeReScheduleToken(struct ADAPTER *prAdapter,
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prReScheduleToken);
static void FreeReScheduleNdlInfo(struct ADAPTER *prAdapter,
	struct _NAN_RESCHED_NDL_INFO *reSchedNdlInfo);

static int
GetActiveNDPNum(const struct _NAN_NDL_INSTANCE_T *prNDL)
{
	uint8_t i = 0;
	uint8_t activeNdpNum = 0;

	if (prNDL == NULL)
		return 0;
	for (i = 0; i < prNDL->ucNDPNum; i++) {
		if (prNDL->arNDP[i].fgNDPActive == TRUE)
			activeNdpNum++;
	}
	return activeNdpNum;
}

static uint8_t ChkEnqCond(enum _ENUM_NDL_MGMT_STATE_T e)
{
	if (e == NDL_REQUEST_SCHEDULE_NDP ||
		e == NDL_REQUEST_SCHEDULE_NDL ||
		e == NDL_SCHEDULE_SETUP ||
		e == NDL_SCHEDULE_ESTABLISHED)
		return TRUE;
	return FALSE;
}

static uint8_t ChkDeqCond(enum _ENUM_NDL_MGMT_STATE_T e)
{
	if (e == NDL_SCHEDULE_ESTABLISHED)
		return TRUE;
	return FALSE;
}

static uint8_t ChkShrinkCond(struct _NAN_RESCHED_NDL_INFO *ReSchedNdlInfo)
{
	uint8_t shrinkCondition = FALSE;

	if (ReSchedNdlInfo == NULL)
		return FALSE;
	shrinkCondition =
		(ReSchedNdlInfo) &&
		(ReSchedNdlInfo->eNdlRescheduleState ==
			NDL_RESCHEDULE_STATE_NEW) &&
		(!ChkDeqCond(ReSchedNdlInfo->prNDL->eCurrentNDLMgmtState));
	return shrinkCondition;
}

static struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T *
GenReScheduleToken(
	struct ADAPTER *prAdapter,
	const uint8_t fgRescheduleInputNDL,
	const enum RESCHEDULE_SOURCE event,
	const struct _NAN_NDL_INSTANCE_T *prNDL)
{

	uint8_t ucNdlIndex;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prReScheduleToken = NULL;
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo =
		&(prAdapter->rDataPathInfo);
	static uint8_t uReSchedTokenID;
	struct _NAN_RESCHED_NDL_INFO *prReSchedNdlInfo = NULL;

	prReScheduleToken =
		(struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T *)
		cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T));

	if (prReScheduleToken) {
		uReSchedTokenID = uReSchedTokenID % 100;
		prReScheduleToken->ucTokenID = uReSchedTokenID++;
		DBGLOG(NAN, INFO, LOGSTR_TOKEN_INFO,
			prReScheduleToken->ucTokenID);
		prReScheduleToken->ucRescheduleEvent = event;
#if (CFG_SUPPORT_NAN_11BE == 1)
		prReScheduleToken->fgIsEhtReschedule = prNDL ?
			prNDL->fgIsEhtReschedule :
			FALSE;
#endif
		LINK_INITIALIZE(&(prReScheduleToken->rReSchedNdlList));
		for (ucNdlIndex = 0; ucNdlIndex < NAN_MAX_SUPPORT_NDL_NUM;
				ucNdlIndex++) {
			if (prDataPathInfo->arNDL[ucNdlIndex].
					fgNDLValid == TRUE &&
				ChkEnqCond(
				prDataPathInfo->arNDL[ucNdlIndex].
				eCurrentNDLMgmtState) == TRUE) {
				if (prNDL != NULL &&
					fgRescheduleInputNDL == FALSE &&
					prNDL ==
						&prDataPathInfo->
						arNDL[ucNdlIndex]) {
					/*
					 * this is NDL triggered this reschedule
					 * (eg., newly connected device.
					 * not existing device)
					 * skip this NDL.
					 */
					continue;
				}
				prReSchedNdlInfo =
					(struct _NAN_RESCHED_NDL_INFO *)
					cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					sizeof(struct _NAN_RESCHED_NDL_INFO));
				if (prReSchedNdlInfo) {
					prReSchedNdlInfo->eNdlRescheduleState =
						NDL_RESCHEDULE_STATE_NEW;
					prReSchedNdlInfo->prNDL =
					&prDataPathInfo->arNDL[ucNdlIndex];
					LINK_INSERT_TAIL(
					&(prReScheduleToken->rReSchedNdlList),
					&(prReSchedNdlInfo->rLinkEntry));
					DBGLOG(NAN, INFO,
					LOGSTR_NDL_INFO,
					ucNdlIndex,
					prReSchedNdlInfo->prNDL->
					aucPeerMacAddr);
				} else {
					DBGLOG(NAN, ERROR,
					"Failed to generate a prReSchedNdlInfo.");
				}
			}
		}
		DBGLOG(NAN, INFO,
			LOGSTR_LVL2_END);
	}
	return prReScheduleToken;
}

uint32_t
ReleaseNanSlotsForSchedulePrep(
	struct ADAPTER *prAdapter,
	const enum RESCHEDULE_SOURCE event,
	u_int8_t fgIsEhtRescheduleNewNDL)
{
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	union _NAN_BAND_CHNL_CTRL rAisChnlInfo = {0};
	uint32_t u4SlotBitmap = 0;
	uint8_t ucAisPhyTypeSet;
	enum ENUM_BAND eAisBand;

	if (event == AIS_CONNECTED) {
		if (nanSchedGetAisChnlUsage(prAdapter, &rAisChnlInfo,
					&u4SlotBitmap, &ucAisPhyTypeSet) !=
				WLAN_STATUS_SUCCESS) {
			rRetStatus = WLAN_STATUS_FAILURE;

		} else {
			eAisBand = nanRegGetNanChnlBand(rAisChnlInfo);
			nanSchedReleaseReschedCommitSlot(prAdapter,
				NAN_SLOT_MASK_TYPE_AIS,
				nanGetTimelineMgmtIndexByBand(prAdapter,
				eAisBand));
		}
	} else if (event == AIS_DISCONNECTED) {
		nanSchedReleaseReschedCommitSlot(prAdapter,
			NAN_SLOT_MASK_TYPE_AIS,
			nanGetTimelineMgmtIndexByBand(prAdapter,
			BAND_2G4));
		nanSchedReleaseReschedCommitSlot(prAdapter,
			NAN_SLOT_MASK_TYPE_AIS,
			nanGetTimelineMgmtIndexByBand(prAdapter,
			BAND_5G));
	} else if (event == NEW_NDL) {
		uint32_t u4ReschedSlot = 0;

#if (CFG_SUPPORT_NAN_11BE == 1)
		u4ReschedSlot = fgIsEhtRescheduleNewNDL ?
			(NAN_SLOT_MASK_TYPE_AIS | NAN_SLOT_MASK_TYPE_NDL) :
			NAN_SLOT_MASK_TYPE_NDL;
#else
		u4ReschedSlot = NAN_SLOT_MASK_TYPE_NDL;
#endif

		nanSchedReleaseReschedCommitSlot(prAdapter,
			u4ReschedSlot,
			nanGetTimelineMgmtIndexByBand(prAdapter,
			BAND_5G));
	} else if (event == REMOVE_NDL) {
/* Only need release if REMOVE_NDL condition recover to customer requirement */
#if 0
		nanSchedReleaseReschedCommitSlot(prAdapter,
			NAN_SLOT_MASK_TYPE_NDL,
			nanGetTimelineMgmtIndexByBand(prAdapter,
			BAND_5G));
#else
		DBGLOG(NAN, WARN, "Not release slot when REMOVE NDL\n");
		/* Not release committed forcely,
		 * but still need to release unused commit slot
		 */
		nanSchedReleaseUnusedCommitSlot(prAdapter);
		nanSchedCmdUpdateAvailability(prAdapter);
#endif
	}
	return rRetStatus;
}


struct _NAN_RESCHED_NDL_INFO*
getOngoing_RescheduleNDL(struct ADAPTER *prAdapter) {
	struct _NAN_RESCHED_NDL_INFO *ReSchedNdlInfo = NULL;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T *reschedToken =
		getOngoing_RescheduleToken(prAdapter);
	if (reschedToken) {
		ReSchedNdlInfo = LINK_PEEK_HEAD(
				&(reschedToken->rReSchedNdlList),
				struct _NAN_RESCHED_NDL_INFO,
				rLinkEntry);
	}
	if (ReSchedNdlInfo &&
		ReSchedNdlInfo->eNdlRescheduleState ==
		NDL_RESCHEDULE_STATE_NEGO_ONGOING)
		return ReSchedNdlInfo;
	else
		return NULL;
}
struct _NAN_RESCHED_NDL_INFO*
getNewState_RescheduleNDL(struct ADAPTER *prAdapter) {
	struct _NAN_RESCHED_NDL_INFO *ReSchedNdlInfo = NULL;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T *reschedToken =
		getOngoing_RescheduleToken(prAdapter);
	if (reschedToken) {
		ReSchedNdlInfo = LINK_PEEK_HEAD(
				&(reschedToken->rReSchedNdlList),
				struct _NAN_RESCHED_NDL_INFO,
				rLinkEntry);
	}
	if (ReSchedNdlInfo &&
		ReSchedNdlInfo->eNdlRescheduleState ==
		NDL_RESCHEDULE_STATE_NEW)
		return ReSchedNdlInfo;
	else
		return NULL;
}

/*
 * getNewState_RescheduleNDL_reorder filters
 * ndl info which is (not started reschedule yet)
 * AND (NDL not yet established)
 */
struct _NAN_RESCHED_NDL_INFO*
getNewState_RescheduleNDL_reorder(struct ADAPTER *prAdapter)
{
	struct _NAN_RESCHED_NDL_INFO *ReSchedNdlInfo = NULL;
	struct _NAN_RESCHED_NDL_INFO *prNDLInfo;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T *reschedToken =
		getOngoing_RescheduleToken(prAdapter);

	if (reschedToken) {
		ReSchedNdlInfo = LINK_PEEK_HEAD(
			&(reschedToken->rReSchedNdlList),
			struct _NAN_RESCHED_NDL_INFO,
			rLinkEntry);
		while (ChkShrinkCond(ReSchedNdlInfo)) {
			LINK_REMOVE_HEAD(&(reschedToken->rReSchedNdlList),
				prNDLInfo, struct _NAN_RESCHED_NDL_INFO*);
			FreeReScheduleNdlInfo(prAdapter, prNDLInfo);
			ReSchedNdlInfo = LINK_PEEK_HEAD(
					&(reschedToken->rReSchedNdlList),
					struct _NAN_RESCHED_NDL_INFO,
					rLinkEntry);
		}
	}
	return getNewState_RescheduleNDL(prAdapter);
}
struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T*
getOngoing_RescheduleToken(struct ADAPTER *prAdapter) {
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
	*prReScheduleToken = NULL;
	prReScheduleToken =
	LINK_PEEK_HEAD(
		&(prAdapter->rDataPathInfo.rReScheduleTokenList),
		struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T,
		rLinkEntry);
	return prReScheduleToken;
}


static void FreeReScheduleToken(struct ADAPTER *prAdapter,
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T *prReScheduleToken)
{
	struct _NAN_RESCHED_NDL_INFO *prReSchedNdlInfo = NULL;

	if (prReScheduleToken == NULL)
		return;
	while (!LINK_IS_EMPTY(&prReScheduleToken->rReSchedNdlList)) {
		LINK_REMOVE_HEAD(
			&prReScheduleToken->rReSchedNdlList,
			prReSchedNdlInfo,
			struct _NAN_RESCHED_NDL_INFO *);
		cnmMemFree(prAdapter, prReSchedNdlInfo);
	}
	cnmMemFree(prAdapter, prReScheduleToken);
}
static void FreeReScheduleNdlInfo(struct ADAPTER *prAdapter,
	struct _NAN_RESCHED_NDL_INFO *reSchedNdlInfo)
{
	cnmMemFree(prAdapter, reSchedNdlInfo);
}
static void handleRemoveNDL(struct ADAPTER *prAdapter,
		enum RESCHEDULE_SOURCE event,
		struct _NAN_NDL_INSTANCE_T *prNDL) {
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prReScheduleToken = NULL;
	struct _NAN_RESCHED_NDL_INFO *prNextNDLInfo;
	struct _NAN_RESCHED_NDL_INFO *prOngoingNDLInfo;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prOngoingReScheduleToken = NULL;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prNextReScheduleToken = NULL;

	prOngoingNDLInfo = getOngoing_RescheduleNDL(prAdapter);
	prOngoingReScheduleToken =
		getOngoing_RescheduleToken(prAdapter);
	if (prNDL && prOngoingNDLInfo &&
			prNDL == prOngoingNDLInfo->prNDL) {
		DBGLOG(NAN, INFO, LOGSTR_LVL3_FAIL,
				MAC2STR(prNDL->aucPeerMacAddr));
		LINK_REMOVE_HEAD(
			&(prOngoingReScheduleToken->rReSchedNdlList),
			prOngoingNDLInfo,
			struct _NAN_RESCHED_NDL_INFO*);
		FreeReScheduleNdlInfo(prAdapter, prOngoingNDLInfo);
		if (LINK_IS_EMPTY(&(prOngoingReScheduleToken->
				rReSchedNdlList)) == FALSE)	{
			/* get next NDL */
			prNextNDLInfo =
				getNewState_RescheduleNDL_reorder(prAdapter);
			if (prNextNDLInfo) {
				prNextNDLInfo->eNdlRescheduleState =
					NDL_RESCHEDULE_STATE_NEGO_ONGOING;
				DBGLOG(NAN, INFO, LOGSTR_LVL3_NEXT,
						MAC2STR(prNextNDLInfo->prNDL->
							aucPeerMacAddr));
				nanUpdateNdlScheduleV2(prAdapter,
						prNextNDLInfo->prNDL);
			}
		} else {
			LINK_REMOVE_HEAD(
			&(prAdapter->rDataPathInfo.
			rReScheduleTokenList),
			prNextReScheduleToken,
			struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T*);
			if (prNextReScheduleToken != NULL)
				DBGLOG(NAN, INFO, LOGSTR_LVL2_DONE,
				prNextReScheduleToken->ucTokenID,
				RESCHEDULE_SOURCE_dbgstr[prNextReScheduleToken->
				ucRescheduleEvent]);
			FreeReScheduleToken(prAdapter,
				prOngoingReScheduleToken);
			if (LINK_IS_EMPTY(
				&(prAdapter->rDataPathInfo.
				rReScheduleTokenList)) == FALSE) {
				prOngoingReScheduleToken =
					getOngoing_RescheduleToken(prAdapter);
				prNextNDLInfo =
					getNewState_RescheduleNDL_reorder(
						  prAdapter);
				if (prNextNDLInfo) {
					prNextNDLInfo->eNdlRescheduleState =
					    NDL_RESCHEDULE_STATE_NEGO_ONGOING;
					if (prOngoingReScheduleToken)
						DBGLOG(NAN, INFO,
						LOGSTR_LVL2_NEXT,
						prOngoingReScheduleToken->
						ucTokenID,
						RESCHEDULE_SOURCE_dbgstr[
						prOngoingReScheduleToken->
						ucRescheduleEvent]);
					DBGLOG(NAN, INFO, LOGSTR_LVL3_NEXT,
					    MAC2STR(prNextNDLInfo->prNDL->
					    aucPeerMacAddr));
					nanUpdateNdlScheduleV2(prAdapter,
					    prNextNDLInfo->prNDL);
				}
			} else {
				DBGLOG(NAN, INFO, LOGSTR_LVL2_NOTOKEN);
			}

		}
	} else {
		DBGLOG(NAN, VOC, LOGSTR_LVL1_CHECKING,
				RESCHEDULE_SOURCE_dbgstr[event]);

		if (nanCheckIsNeedReschedule(prAdapter, event, NULL)) {
			/* create reschedToken with event */
			prReScheduleToken =
			GenReScheduleToken(prAdapter, FALSE, event, NULL);
			if (prReScheduleToken)
				DBGLOG(NAN, INFO, LOGSTR_LVL2_GENTOKEN,
				prReScheduleToken->ucTokenID,
				RESCHEDULE_SOURCE_dbgstr[event]);
			if (prReScheduleToken &&
				LINK_IS_EMPTY(
				&prReScheduleToken->rReSchedNdlList) == FALSE) {
				ReleaseNanSlotsForSchedulePrep(prAdapter,
					event, FALSE);
				/* enqueue NDLs to be rescheduled */
				LINK_INSERT_TAIL(
					&prAdapter->rDataPathInfo.
					rReScheduleTokenList,
					&prReScheduleToken->rLinkEntry);
				prNextNDLInfo =
				getNewState_RescheduleNDL_reorder(prAdapter);
				if (prNextNDLInfo) {
					prNextNDLInfo->eNdlRescheduleState =
					    NDL_RESCHEDULE_STATE_NEGO_ONGOING;
					DBGLOG(NAN, VOC, LOGSTR_LVL3_START,
					    MAC2STR(prNextNDLInfo->prNDL->
					    aucPeerMacAddr));
					nanUpdateNdlScheduleV2(prAdapter,
						prNextNDLInfo->prNDL);
				}
			} else {
				if (prReScheduleToken) {
					DBGLOG(NAN, INFO, LOGSTR_LVL2_NONDL,
					prReScheduleToken->ucTokenID,
					RESCHEDULE_SOURCE_dbgstr[event]);
					FreeReScheduleToken(prAdapter,
						prReScheduleToken);
				}
			}
		} else {
			DBGLOG(NAN, INFO, LOGSTR_LVL1_NONEED);
		}
	}

}
static void handleNewNDL(struct ADAPTER *prAdapter,
	enum RESCHEDULE_SOURCE event, struct _NAN_NDL_INSTANCE_T *prNDL) {
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prReScheduleToken = NULL;
	struct _NAN_RESCHED_NDL_INFO *prNextNDLInfo;
	struct _NAN_RESCHED_NDL_INFO *prOngoingNDLInfo;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prOngoingReScheduleToken = NULL;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prNextReScheduleToken = NULL;
	uint8_t ucNDPNum;

	ucNDPNum = GetActiveNDPNum(prNDL);
	prOngoingNDLInfo = getOngoing_RescheduleNDL(prAdapter);
	prOngoingReScheduleToken = getOngoing_RescheduleToken(prAdapter);

	if ((ucNDPNum > 0) && (prOngoingNDLInfo) &&
		(prOngoingNDLInfo->prNDL  == prNDL) &&
		(prNDL->eNDLRole == NAN_PROTOCOL_INITIATOR)) {
		/*
		 * In this case : input prNDL is result from
		 * "NEW_NDL initiated NDL reschedule".
		 * so do not generate reScheduleToken again.
		 * but process Ongoing reScheduleToken.
		 */
		DBGLOG(NAN, VOC, LOGSTR_LVL3_DONE,
			MAC2STR(prNDL->aucPeerMacAddr));
		prOngoingNDLInfo->eNdlRescheduleState =
			NDL_RESCHEDULE_STATE_ESTABLISHED;
		/* dequeue completed NDL from rReSchedNdlList. */
		LINK_REMOVE_HEAD(&(prOngoingReScheduleToken->rReSchedNdlList),
			prNextNDLInfo, struct _NAN_RESCHED_NDL_INFO*);
		FreeReScheduleNdlInfo(prAdapter, prOngoingNDLInfo);


		/* reschedule next NDL from reschedule queue(rReSchedNdlList) */
		if (LINK_IS_EMPTY(
			&(prOngoingReScheduleToken->rReSchedNdlList)) ==
			FALSE) {
			/* -> get next NDL */
			prNextNDLInfo =
				getNewState_RescheduleNDL_reorder(prAdapter);
			if (prNextNDLInfo) {
				prNextNDLInfo->eNdlRescheduleState =
					NDL_RESCHEDULE_STATE_NEGO_ONGOING;
				DBGLOG(NAN, INFO, LOGSTR_LVL3_NEXT,
						MAC2STR(prNextNDLInfo->prNDL->
						aucPeerMacAddr));
				nanUpdateNdlScheduleV2(prAdapter,
				prNextNDLInfo->prNDL);
			}
		} else {
			LINK_REMOVE_HEAD(
			&(prAdapter->rDataPathInfo.rReScheduleTokenList),
			prNextReScheduleToken,
			struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T*);
			if (prNextReScheduleToken != NULL)
				DBGLOG(NAN, INFO, LOGSTR_LVL2_DONE,
				prNextReScheduleToken->ucTokenID,
				RESCHEDULE_SOURCE_dbgstr[prNextReScheduleToken->
				ucRescheduleEvent]);
			FreeReScheduleToken(prAdapter,
				prOngoingReScheduleToken);
			/* 1. dequeue next ReschedToken from
			 * &prAdapter->rDataPathInfo.
			 * rReScheduleTokenList.
			 * And  reschedule new NDL.
			 */
			if (LINK_IS_EMPTY(
					&(prAdapter->rDataPathInfo.
					rReScheduleTokenList)) == FALSE) {
				prNextReScheduleToken =
					getOngoing_RescheduleToken(prAdapter);
				prNextNDLInfo =
					getNewState_RescheduleNDL_reorder(
							  prAdapter);
				if (prNextNDLInfo) {
					prNextNDLInfo->eNdlRescheduleState =
					NDL_RESCHEDULE_STATE_NEGO_ONGOING;
					if (prNextReScheduleToken)
						DBGLOG(NAN, INFO,
						LOGSTR_LVL2_NEXT,
						prNextReScheduleToken->
						ucTokenID,
						RESCHEDULE_SOURCE_dbgstr[
						prNextReScheduleToken->
						ucRescheduleEvent]);
					DBGLOG(NAN, INFO, LOGSTR_LVL3_NEXT,
					MAC2STR(prNextNDLInfo->prNDL->
					aucPeerMacAddr));
					nanUpdateNdlScheduleV2(prAdapter,
						prNextNDLInfo->prNDL);
				}
			} else {
				DBGLOG(NAN, INFO, LOGSTR_LVL2_NOTOKEN);
			}

		}
	} else if ((ucNDPNum > 0) && (prOngoingNDLInfo) &&
		(prOngoingNDLInfo) && (prOngoingNDLInfo->prNDL != prNDL) &&
		(prNDL->eNDLRole == NAN_PROTOCOL_INITIATOR)) {
		DBGLOG(NAN, INFO, LOGSTR_ETC1);
	} else if ((ucNDPNum > 0) &&  (prOngoingNDLInfo == NULL) &&
		prNDL->eNDLRole == NAN_PROTOCOL_RESPONDER) {
		/*
		 * In this case : input prNDL is result from
		 * peer initiated Schedule update
		 */
		DBGLOG(NAN, INFO, LOGSTR_ETC2);
	} else if (ucNDPNum == 0) {
		/*
		 * in this case : this is real NDL creation
		 * from new connection(not by reschedule)
		 */
		DBGLOG(NAN, VOC, LOGSTR_LVL1_CHECK_PENDING,
			RESCHEDULE_SOURCE_dbgstr[event]);
		prNextNDLInfo =
		getNewState_RescheduleNDL_reorder(prAdapter);
		if (prNextNDLInfo != NULL) {
			prReScheduleToken =
				getOngoing_RescheduleToken(prAdapter);
			if (prReScheduleToken &&
				LINK_IS_EMPTY(
				&prReScheduleToken->rReSchedNdlList) == FALSE) {
				prNextNDLInfo->eNdlRescheduleState =
					NDL_RESCHEDULE_STATE_NEGO_ONGOING;
				DBGLOG(NAN, VOC, LOGSTR_LVL3_START,
				MAC2STR(prNextNDLInfo->prNDL->
					aucPeerMacAddr));
				nanUpdateNdlScheduleV2(prAdapter,
					prNextNDLInfo->prNDL);
			}
		} else {
			DBGLOG(NAN, VOC, LOGSTR_LVL1_NOTOKEN);
		}
	} /* ucNDPNum == 0 */

}
void nanRescheduleNdlIfNeeded(struct ADAPTER *prAdapter,
	enum RESCHEDULE_SOURCE event,
	struct _NAN_NDL_INSTANCE_T *prNDL)
{
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo = NULL;
	struct LINK *prReScheduleTokenList = NULL;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prReScheduleToken = NULL;
	struct _NAN_RESCHED_NDL_INFO *prNextNDLInfo;
	struct _NAN_RESCHED_NDL_INFO *prOngoingNDLInfo;

	prDataPathInfo = &(prAdapter->rDataPathInfo);
	prReScheduleTokenList =
		&(prDataPathInfo->rReScheduleTokenList);
	prOngoingNDLInfo =
		getOngoing_RescheduleNDL(prAdapter);

	DBGLOG(NAN, INFO, "[%s] Enter event=%u\n", __func__, event);
	if (prDataPathInfo->ucNDLNum == 0 &&
		prOngoingNDLInfo == NULL) {
		DBGLOG(NAN, VOC, LOGSTR_LVL1_SKIP,
			RESCHEDULE_SOURCE_dbgstr[event]);
		return;
	}
	switch (event) {
	case AIS_CONNECTED:
	/*
	 * "case AIS_DISCONNECTED:"
	 * is not handleed temporarily
	 */
		DBGLOG(NAN, VOC, LOGSTR_LVL1_CHECKING,
			RESCHEDULE_SOURCE_dbgstr[event]);
		if (nanCheckIsNeedReschedule(prAdapter, event, NULL)) {
			prReScheduleToken =
			GenReScheduleToken(prAdapter, FALSE, event, NULL);
			if (prReScheduleToken)
			DBGLOG(NAN, INFO, LOGSTR_LVL2_GENTOKEN,
				prReScheduleToken->ucTokenID,
				RESCHEDULE_SOURCE_dbgstr[event]);
			if (prReScheduleToken &&
				LINK_IS_EMPTY(
				&prReScheduleToken->rReSchedNdlList) == FALSE) {
				ReleaseNanSlotsForSchedulePrep(prAdapter,
					event, FALSE);

				LINK_INSERT_TAIL(prReScheduleTokenList,
					&(prReScheduleToken->rLinkEntry));
				prNextNDLInfo =
				getNewState_RescheduleNDL_reorder(prAdapter);
				/*
				 * if there is ongoing reschedule ahead,
				 * it just push it back and processes
				 * it later. else case handle it directly
				 */
				if (prNextNDLInfo) {
					prNextNDLInfo->eNdlRescheduleState =
					    NDL_RESCHEDULE_STATE_NEGO_ONGOING;
					DBGLOG(NAN, VOC, LOGSTR_LVL3_START,
					    MAC2STR(prNextNDLInfo->prNDL->
					    aucPeerMacAddr));
					nanUpdateNdlScheduleV2(prAdapter,
					    prNextNDLInfo->prNDL);
				}
			} else {
				DBGLOG(NAN, INFO, LOGSTR_LVL2_NONDL,
					prReScheduleToken->ucTokenID,
					RESCHEDULE_SOURCE_dbgstr[event]);
				FreeReScheduleToken(prAdapter,
					prReScheduleToken);
			}
		} else {
			DBGLOG(NAN, INFO, LOGSTR_LVL1_NONEED);
		}
		break;

	case REMOVE_NDL:
		handleRemoveNDL(prAdapter, event, prNDL);
		break;

	case NEW_NDL:
		handleNewNDL(prAdapter, event, prNDL);
		break;
	default:
		break;
	}
}

void nanRescheduleEnqueueNewToken(struct ADAPTER *prAdapter,
	enum RESCHEDULE_SOURCE event,
	struct _NAN_NDL_INSTANCE_T *prNDL) {
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
		*prReScheduleToken = NULL;
	DBGLOG(NAN, VOC, LOGSTR_LVL1_TRIG,
		RESCHEDULE_SOURCE_dbgstr[event]);
	prReScheduleToken =
		GenReScheduleToken(prAdapter, FALSE, event, prNDL);
	if (prReScheduleToken) {
		DBGLOG(NAN, INFO, LOGSTR_LVL2_GENTOKEN,
				prReScheduleToken->ucTokenID,
				RESCHEDULE_SOURCE_dbgstr[event]);
		if (LINK_IS_EMPTY(
			&prReScheduleToken->rReSchedNdlList) == FALSE) {
			LINK_INSERT_TAIL(
				&(prAdapter->rDataPathInfo.
				rReScheduleTokenList),
				&prReScheduleToken->rLinkEntry);
		} else {
			DBGLOG(NAN, INFO, LOGSTR_LVL2_NONDL,
			prReScheduleToken->ucTokenID,
			RESCHEDULE_SOURCE_dbgstr[event]);
			FreeReScheduleToken(prAdapter,
				prReScheduleToken);
		}
	} else {
		DBGLOG(NAN, ERROR,
			"Failed to generate a ReScheduleToken.");
	}
}

enum RESCHEDULE_SOURCE
nanRescheduleGetOngoingTokenEvent(struct ADAPTER *prAdapter) {
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T
	*reschedToken = NULL;

	reschedToken =
		getOngoing_RescheduleToken(prAdapter);
	if (reschedToken)
		return reschedToken->ucRescheduleEvent;
	else
		return RESCHEDULE_NULL;
}

void
nanRescheduleInit(struct ADAPTER *prAdapter) {
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo;

	DBGLOG(NAN, INFO, LOGSTR_INIT);
	prDataPathInfo = &(prAdapter->rDataPathInfo);
	LINK_INITIALIZE(&(prDataPathInfo->rReScheduleTokenList));
	nanSchedRegisterReschedInf(getOngoing_RescheduleToken);
}

void
nanRescheduleDeInit(struct ADAPTER *prAdapter) {
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo;
	struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T *prReScheduleToken;

	prDataPathInfo = &(prAdapter->rDataPathInfo);
	DBGLOG(NAN, INFO, LOGSTR_DEINIT);
	while (!LINK_IS_EMPTY(&prDataPathInfo->rReScheduleTokenList)) {
		LINK_REMOVE_HEAD(
		&prDataPathInfo->rReScheduleTokenList, prReScheduleToken,
		struct _NAN_DATA_ENGINE_SCHEDULE_RESCHEDULE_TOKEN_T *);
		FreeReScheduleToken(prAdapter, prReScheduleToken);
	}
	nanSchedUnRegisterReschedInf();
}

void nanResumeRescheduleTimeout(struct ADAPTER *prAdapter, uintptr_t ulParam)
{
	/* handleRemoveNDL(prAdapter, REMOVE_NDL, NULL); */
}
#endif /* CFG_SUPPORT_NAN_RESCHEDULE  */

