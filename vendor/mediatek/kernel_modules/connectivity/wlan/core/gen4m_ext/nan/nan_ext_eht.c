/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext_eht.c
 */

/*! \file   "nan_ext_eht.c"
 *  \brief  This file defines the procedures handling EHT commands.
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


#include "nan_ext_eht.h"

#if CFG_SUPPORT_NAN_EXT

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define NAN_DEFAULT_ENABLE_EHT (1)

#define NAN_6G_BW20_DEFAULT_CHANNEL	5

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

static struct IE_NAN_EHT_PENDING_CMD g_nanEhtPendingCmd = {0};

#if (CFG_SUPPORT_NAN_6G == 1)
#if (CFG_SUPPORT_NAN_11BE == 1)
static uint8_t eht_mode = 0xff;
#endif
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
const char *eht_type_str(uint8_t i)
{
	static const char * const eht_type_string[] = {
		[0] = "Request",
		[1] = "Response",
		[2] = "Confirm",
	};

	if (likely(i < ARRAY_SIZE(eht_type_string)))
		return eht_type_string[i];

	return "Invalid";
}

static const char *eht_mode_str(uint8_t i)
{
	static const char * const eht_mode_string[] = {
		[0] = "Legacy",
		[1] = "Mode 1",
		[2] = "Mode 2",
		[3] = "Mode 3",
		[4] = "Mode 4",
	};

	if (likely(i < ARRAY_SIZE(eht_mode_string)))
		return eht_mode_string[i];

	return "Invalid";
}

void nanSetEhtModeCtrl(struct ADAPTER *prAdapter, uint8_t mode)
{
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo = {0};
	uint8_t cmd[30] = {0};
	uint8_t strLen = 0;
	uint32_t strOutLen = 0;
	uint8_t bw = 0;

	if (mode)
		bw = BW_320;
	else
		bw = BW_160;

	strLen = kalSnprintf(cmd, sizeof(cmd),
			"NanSched set %d %d",
			0x0b0b,
			bw);
	DBGLOG(NAN, INFO,
		"EHT VSIE Notify FW %s, strlen=%d\n",
		cmd, strLen);

	rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_ASCII;
	rChipConfigInfo.u2MsgSize = strLen;
	kalStrnCpy(rChipConfigInfo.aucCmd, cmd, strLen);
	wlanSetChipConfig(prAdapter, &rChipConfigInfo,
			sizeof(rChipConfigInfo), &strOutLen, FALSE);
}

static void freePendingEhtCmd(struct ADAPTER *prAdapter,
			   struct IE_NAN_EHT_PENDING_CMD *prPendingCmd)
{
	DBGLOG(NAN, INFO, "Enter %s, free pending request %u\n",
	       __func__,
	       ((struct IE_NAN_EHT_CMD *)(prPendingCmd->cmd))->ucRequestId);

	kalMemZero(prPendingCmd, sizeof(struct IE_NAN_EHT_PENDING_CMD));

}

static uint32_t savePendingEhtCmd(struct ADAPTER *prAdapter,
		      struct IE_NAN_EHT_CMD *cmd)
{
	struct IE_NAN_EHT_PENDING_CMD *prPendingCmd = &g_nanEhtPendingCmd;

	DBGLOG(NAN, INFO,
	       "req=%u, %u %u\n",
	       cmd->ucRequestId,
	       cmd->type,
	       cmd->mode);

	prPendingCmd->ucNanOui = cmd->ucNanOui;

	kalMemCopy(prPendingCmd->cmd, cmd, EXT_MSG_SIZE(cmd));

	return WLAN_STATUS_SUCCESS;
}

uint32_t nanComposeEHTResponse(struct ADAPTER *prAdapter,
				struct IE_NAN_EHT_CMD *cmd,
				uint8_t ucRequestId,
				enum NAN_EHT_PACKET_TYPE type,
				enum NAN_EHT_REASON reason,
				enum NAN_EHT_MODE mode)
{
	struct IE_NAN_EHT_EVENT *prResponse;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(NAN, INFO,
		"Enter %s, request=%u type=%u reason=%u mode=%u\n",
		__func__, ucRequestId, type, reason, mode);

	prResponse = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				 NAN_MAX_EXT_DATA_SIZE);
	if (!prResponse) {
		rStatus = WLAN_STATUS_FAILURE;
		goto done;
	}

	prResponse->ucNanOui = NAN_EHT_EVT_OUI;
	prResponse->u2Length = sizeof(struct IE_NAN_EHT_EVENT) -
				EHT_EVENT_HDR_LEN;

	DBGLOG(NAN, TRACE,
		"u2Length = %u (%zu-%u)\n",
		prResponse->u2Length,
		sizeof(struct IE_NAN_EHT_EVENT),
		EHT_EVENT_HDR_LEN);

	/* Larger than allocated buffer size */
	if (EXT_MSG_SIZE(prResponse) > NAN_MAX_EXT_DATA_SIZE) {
		rStatus = WLAN_STATUS_FAILURE;
		goto done;
	}

	prResponse->ucMajorVersion = 2;
	prResponse->ucMinorVersion = 1;

	prResponse->ucRequestId = ucRequestId;
	prResponse->type = type;
	prResponse->reason = reason;
	prResponse->mode = mode;

	DBGLOG(NAN, TRACE, "OUI: %d(%02X) (%s)\n",
	       prResponse->ucNanOui, prResponse->ucNanOui,
	       oui_str(prResponse->ucNanOui));
	DBGLOG(NAN, TRACE, "Length: %d\n", prResponse->u2Length);
	DBGLOG(NAN, TRACE, "v%d.%d\n",
	       prResponse->ucMajorVersion, prResponse->ucMinorVersion);
	DBGLOG(NAN, TRACE, "ReqId: %d\n", prResponse->ucRequestId);
	DBGLOG(NAN, TRACE, "Type: %d (%s)\n",
	       prResponse->type, eht_type_str(prResponse->type));
	DBGLOG(NAN, TRACE, "Reason: %d\n",
	       prResponse->reason);
	DBGLOG(NAN, INFO, "Mode: %d (%s)\n",
	       prResponse->mode, eht_mode_str(prResponse->mode));

	/* Reuse the buffer from passed in from HAL */
	DBGLOG(NAN, TRACE, "Copy %zu bytes\n", EXT_MSG_SIZE(prResponse));

	if (cmd)
		kalMemCopy(cmd, prResponse, EXT_MSG_SIZE(prResponse));
	else
		nanExtSendIndication(prAdapter, prResponse, EXT_MSG_SIZE(prResponse));

done:
	if (prResponse)
		cnmMemFree(prAdapter, prResponse);

	return rStatus;
}

void nanClearEhtStaRec(struct STA_RECORD *s)
{
#if (CFG_SUPPORT_802_11BE == 1)
	static uint8_t zero[EHT_PHY_CAP_BYTE_NUM];

	kalMemCopy(s->ucEhtMacCapInfo, zero,
		EHT_MAC_CAP_BYTE_NUM);
	kalMemCopy(s->ucEhtPhyCapInfo, zero,
		EHT_PHY_CAP_BYTE_NUM);
#if 0
	kalMemCopy(s->ucEhtPhyCapInfoExt, zero
		EHT_PHY_CAP_BYTE_NUM);
	kalMemCopy(s->aucMcsMap20MHzSta, zero,
		sizeof(s->aucMcsMap20MHzSta));
	kalMemCopy(s->aucMcsMap80MHz, zero,
		sizeof(s->aucMcsMap80MHz));
	kalMemCopy(s->aucMcsMap160MHz, zero,
		sizeof(s->aucMcsMap160MHz));
	kalMemCopy(s->aucMcsMap320MHz, zero,
		sizeof(s->aucMcsMap320MHz));
#endif
#endif
}


void nanEnablePeerEhtMode(
	struct ADAPTER *ad,
	uint8_t enable)
{
#if (CFG_SUPPORT_NAN_6G == 1)
#if (CFG_SUPPORT_NAN_11BE == 1)
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo =
		(struct _NAN_DATA_PATH_INFO_T *)NULL;
	struct _NAN_NDL_INSTANCE_T *prNDL =
		(struct _NAN_NDL_INSTANCE_T *)NULL;
	struct _NAN_NDP_INSTANCE_T *prNDP =
		(struct _NAN_NDP_INSTANCE_T *)NULL;
	struct _NAN_NDP_CONTEXT_T *prNdpCxt =
		(struct _NAN_NDP_CONTEXT_T *)NULL;
	uint32_t i, j;
	struct STA_RECORD *s =
		(struct STA_RECORD *)NULL;
	struct BSS_INFO *b =
		(struct BSS_INFO *)NULL;

	if (!nanIsEhtSupport(ad)) {
		DBGLOG(NAN, INFO,
			"Not support eht mode\n");
		return;
	}

	/* If have NDP, update STAREC */
	prDataPathInfo = &(ad->rDataPathInfo);
	for (i = 0; i < NAN_MAX_SUPPORT_NDL_NUM; i++){
		prNDL = &(prDataPathInfo->arNDL[i]);
		if (!prNDL || prNDL->fgNDLValid == FALSE) {
			continue;
		}

		for (j = 0; j < NAN_MAX_SUPPORT_NDP_NUM; j++){
			prNDP  = &(prNDL->arNDP[j]);
			if (!prNDP || prNDP->fgNDPValid == FALSE)
				continue;

			DBGLOG(NAN, INFO,
				"Peer NDL[" MACSTR "], NDP[" MACSTR "]\n",
				MAC2STR(prNDL->aucPeerMacAddr),
				MAC2STR(prNDP->aucPeerNDIAddr));

			for (int k = 0;
				k < NAN_MAX_SUPPORT_NDP_CXT_NUM;
				k++) {
				prNdpCxt = &prNDL->arNdpCxt[k];
				if (!prNdpCxt->prNanStaRec) {
					continue;
				}

				s = prNdpCxt->prNanStaRec;
				b = GET_BSS_INFO_BY_INDEX(
					ad,
					s->ucBssIndex);

				if (enable) {
					prNDL->ucPhyTypeSet |= PHY_TYPE_BIT_EHT;
					ehtRlmNANFillCapIE(
						ad, b,
						prNDL->aucIeEhtCap);
				} else {
					prNDL->ucPhyTypeSet &= ~PHY_TYPE_BIT_EHT;
					kalMemZero(&(prNDL->aucIeEhtCap),
						sizeof(prNDL->aucIeEhtCap));
					nanClearEhtStaRec(s);
				}

				nanDataEngineSetupStaRec(ad,
					prNDL,
					s);

				cnmStaRecChangeState(ad,
					s,
					STA_STATE_3);

				DBGLOG(NAN, INFO,
					"Mode[%u] s[%u %u] b[%u]\n",
					enable,
					s->ucIndex,
					s->ucWlanIndex,
					s->ucBssIndex);
			}
		}
	}
#endif
#endif
}

void nanSetEhtMode(
	struct ADAPTER *ad,
	uint8_t mode)
{
#if (CFG_SUPPORT_NAN_6G == 1)
#if (CFG_SUPPORT_NAN_11BE == 1)
	if (mode) {
		eht_mode = mode;
		ad->rWifiVar.ucNanEht = 2;
	} else {
		eht_mode = NAN_EHT_MODE_LEGACY;
		ad->rWifiVar.ucNanEht = 1;
	}

	/* Always set to BW160 */
	/* ad->rWifiVar.ucNan6gBandwidth = MAX_BW_160MHZ; */
	/* ad->rWifiVar.ucNanEnable6g = 1; */

	/* Sync bw to FW */
	nanSetEhtModeCtrl(ad, eht_mode);
#endif
#endif
}

void nanSet6gConfig(struct ADAPTER *ad)
{
	struct _NAN_SCHEDULER_T *s =
		nanGetScheduler(ad);

	if (!ad || !g_ucNanIsOn)
		return;

	DBGLOG(NAN, INFO,
		"NanEnable6g: %u, fgEn6g: %u \n",
		ad->rWifiVar.ucNanEnable6g,
		s->fgEn6g);

	nanSchedConfigAllowedBand(
		ad,
		s->fgEn2g,
		s->fgEn5gH,
		s->fgEn5gL,
		ad->rWifiVar.ucNanEnable6g);

	nanSchedCmdUpdatePotentialChnlList(ad);
}

void nanEnableEhtMode(
	struct ADAPTER *ad,
	uint8_t mode)
{
#if (CFG_SUPPORT_NAN_6G == 1)
#if (CFG_SUPPORT_NAN_11BE == 1)
	struct _NAN_SCHEDULER_T *prNanScheduler = nanGetScheduler(ad);

	if (!ad || !ad->rWifiVar.ucNanEnable6g || !prNanScheduler->fgEn6g)
		return;

	if (eht_mode == mode) {
		DBGLOG(NAN, INFO,
			"Already in mode: %d\n",
			mode);
		return;
	}

	if (!nanIsEhtSupport(ad)) {
		DBGLOG(NAN, INFO,
			"Not support eht mode\n");
		return;
	}

	nanSetEhtMode(ad, mode);

	/* Reconfig bw */
	nanSchedConfigAllowedBand(
		ad,
		prNanScheduler->fgEn2g,
		prNanScheduler->fgEn5gH,
		prNanScheduler->fgEn5gL,
		ad->rWifiVar.ucNanEnable6g);

	nanSchedCmdUpdatePotentialChnlList(ad);

	if (mode) {
		nanEnablePeerEhtMode(ad, TRUE);
	} else {
		nanEnablePeerEhtMode(ad, FALSE);
	}

#endif
#endif
}

void nanEnableEht(
	struct ADAPTER *ad,
	uint8_t enable)
{
	if (enable)
		nanEnableEhtMode(ad, NAN_EHT_MODE_2);
	else
		nanEnableEhtMode(ad, NAN_EHT_MODE_LEGACY);
}

void nanPeerReportEhtEvent(
	struct ADAPTER *ad,
	uint8_t enable)
{
#if (CFG_SUPPORT_NAN_6G == 1)
#if (CFG_SUPPORT_NAN_11BE == 1)
	struct IE_NAN_EHT_PENDING_CMD *p =
		&g_nanEhtPendingCmd;

	if (nanIsEhtEnable(ad)) {
		struct IE_NAN_EHT_CMD *c =
			(struct IE_NAN_EHT_CMD *)p->cmd;

		nanComposeEHTResponse(ad,
			NULL,
			c->ucRequestId,
			NAN_EHT_PACKET_CONFIRM,
			NAN_EHT_REASON_CONFIRM_OPMODE,
			c->mode);
	} else if (enable) {
		struct IE_NAN_EHT_CMD *c =
			(struct IE_NAN_EHT_CMD *)p->cmd;

		nanComposeEHTResponse(ad,
			NULL,
			c->ucRequestId,
			NAN_EHT_PACKET_CONFIRM,
			NAN_EHT_REASON_CONFIRM_NDP,
			NAN_EHT_MODE_2);
	} else {
		struct IE_NAN_EHT_CMD *c =
			(struct IE_NAN_EHT_CMD *)p->cmd;

		nanComposeEHTResponse(ad,
			NULL,
			c->ucRequestId,
			NAN_EHT_PACKET_CONFIRM,
			NAN_EHT_REASON_CONFIRM_PEER_NO_EHT,
			c->mode);
	}
#endif
#endif
}

uint32_t nanProcessEhtModeCommand(
	struct ADAPTER *prAdapter,
	const uint8_t *buf,
	size_t u4Size)
{
	uint32_t r = WLAN_STATUS_SUCCESS;
	struct IE_NAN_EHT_CMD *c = (struct IE_NAN_EHT_CMD *)buf;

	DBGLOG(NAN, TRACE, "Enter %s, consuming %zu bytes\n",
		   __func__, sizeof(struct IE_NAN_EHT_CMD));

	DBGLOG_HEX(NAN, TRACE, buf, sizeof(struct IE_NAN_EHT_CMD));

	DBGLOG(NAN, TRACE, "OUI: %d(%02X) (%s)\n",
		c->ucNanOui, c->ucNanOui, oui_str(c->ucNanOui));
	DBGLOG(NAN, TRACE, "Length: %d\n", c->u2Length);
	DBGLOG(NAN, TRACE, "v%d.%d\n", c->ucMajorVersion, c->ucMinorVersion);
	DBGLOG(NAN, TRACE, "ReqId: %d\n", c->ucRequestId);
	DBGLOG(NAN, TRACE, "Type: %d (%s)\n",
		c->type, eht_type_str(c->type));
	DBGLOG(NAN, INFO, "Mode: %d\n", c->mode);

	if (g_ucNanIsOn) {
		DBGLOG(NAN, INFO, "Proceed to send command\n");
#if (CFG_SUPPORT_NAN_6G == 1)
		if (c->mode == NAN_EHT_MODE_LEGACY) {
			nanEnableEht(prAdapter, FALSE);
		}
#if (CFG_SUPPORT_NAN_11BE == 1)
		else if (c->mode == NAN_EHT_MODE_2) {
			nanEnableEht(prAdapter, TRUE);
		}
#endif
#endif
	} else { /* used later for sending response */
		DBGLOG(NAN, INFO, "Save command to pending list\n");

		r = savePendingEhtCmd(prAdapter, c);
		if (r != WLAN_STATUS_SUCCESS)
			return WLAN_STATUS_FAILURE;
	}

	return r;
}

/**
 * nanProcessEHT() - Process EHT command from framework
 * @prAdapter: pointer to adapter
 * @buf: buffer holding command and to be filled with response message
 * @u4Size: buffer size of passsed-in buf
 *
 * Context:
 *   The response shall be filled back into the buf in binary format
 *   to be replied to framework,
 *   The binary array will be translated to hexadecimal string in
 *   nanExtParseCmd().
 *   The u4Size shal be checked when filling the response message.
 *
 * Return:
 * WLAN_STATUS_SUCCESS on success;
 * WLAN_STATUS_FAILURE on fail;
 * WLAN_STATUS_PENDING to wait for event from firmware and run OID complete
 */
uint32_t nanProcessEHT(struct ADAPTER *prAdapter, const uint8_t *buf,
		       size_t u4Size)
{
	uint32_t r = WLAN_STATUS_SUCCESS;
	struct IE_NAN_EHT_CMD *c = (struct IE_NAN_EHT_CMD *)buf;

	r = nanProcessEhtModeCommand(prAdapter, buf, u4Size);
	if (r != WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;

#if (CFG_SUPPORT_NAN_6G == 1)
	if (c->mode == NAN_EHT_MODE_LEGACY) {
		r = nanComposeEHTResponse(prAdapter,
			c,
			c->ucRequestId,
			NAN_EHT_PACKET_RESPONSE,
			NAN_EHT_REASON_RESPONSE_OPMODE,
			c->mode);
	}
#if (CFG_SUPPORT_NAN_11BE == 1)
	else if (c->mode == NAN_EHT_MODE_2) {
		r = nanComposeEHTResponse(prAdapter,
			c,
			c->ucRequestId,
			NAN_EHT_PACKET_RESPONSE,
			NAN_EHT_REASON_RESPONSE_VSIE,
			c->mode);
	}
#endif
	else
#endif
		r = nanComposeEHTResponse(prAdapter,
			c,
			c->ucRequestId,
			NAN_EHT_PACKET_RESPONSE,
			NAN_EHT_REASON_RESPONSE_NOT_SUPPORTED,
			c->mode);

	return r;
}

static void nanEhtResetSettings(struct ADAPTER *prAdapter)
{

}

static void nanEhtClearPendingCommands(struct ADAPTER *prAdapter)
{
	struct IE_NAN_EHT_PENDING_CMD *prPendingCmd = &g_nanEhtPendingCmd;

	freePendingEhtCmd(prAdapter, prPendingCmd);
}

static void nanEhtProcessAllPendingCommands(struct ADAPTER *ad)
{
	struct IE_NAN_EHT_PENDING_CMD *prPendingCmd = &g_nanEhtPendingCmd;
	uint8_t fgIsNAN6GChnlAllowed =
			rlmDomainIsLegalChannel(ad, BAND_6G,
						NAN_6G_BW20_DEFAULT_CHANNEL);

	if (prPendingCmd->ucNanOui) {
		DBGLOG(NAN, TRACE, "Process cmd at %p, size=%zu\n",
			prPendingCmd->cmd, EXT_MSG_SIZE(prPendingCmd->cmd));
		nanProcessEhtModeCommand(ad, prPendingCmd->cmd,
			EXT_MSG_SIZE(prPendingCmd->cmd));
		freePendingEhtCmd(ad, prPendingCmd);
	}
#if NAN_DEFAULT_ENABLE_EHT
	else if (ad && ad->rWifiVar.ucNanEnable6g && fgIsNAN6GChnlAllowed) {
		if (nanIsEhtEnable(ad))
			nanSetEhtMode(ad, NAN_EHT_MODE_2);
	}
#endif
}

static void nanEhtReset(struct ADAPTER *prAdapter)
{
	nanEhtResetSettings(prAdapter);
	nanEhtClearPendingCommands(prAdapter);
}

void nanEhtNanOffHandler(struct ADAPTER *prAdapter)
{
	nanEhtReset(prAdapter);
}

void nanEhtNanOnHandler(struct ADAPTER *prAdapter)
{
#if (CFG_SUPPORT_NAN_6G == 1)
#if (CFG_SUPPORT_NAN_11BE == 1)
	eht_mode = 0xff;
#endif
#endif
	nanEhtProcessAllPendingCommands(prAdapter);
}

#endif
