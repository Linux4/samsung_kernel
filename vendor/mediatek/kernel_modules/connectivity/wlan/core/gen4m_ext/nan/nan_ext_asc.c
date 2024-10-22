/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext_asc.c
 */

/*! \file   "nan_ext_asc.c"
 *  \brief  This file defines the procedures handling ASC commands.
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


#include "nan_ext_asc.h"
#include "nan_ext_ascc.h"


#if CFG_SUPPORT_NAN_EXT
static struct NAN_ASC_PENDING_CMD g_nanAscPendingCmd;
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_EXT_CMD_ASC_T {
	uint8_t ucType;
	/* 0: Request
	 * 1: Response
	 */
	uint8_t ucSubtype;
	/* 0: Return to original NAN
	 * 1: Another NAN Cluster
	 * 2: Connected Access Point (AP)
	 * 3: Other proxy (Beaconing device)
	 * 4: Maintain current NAN and take a different DW Offset
	 */
	uint8_t ucPowerSave;
	/* 0: Maintain current NAN role
	 * 1: Master becomes NMNS
	 */
	uint8_t ucTrackEnable;
	/* 0: Disable or don't care
	 * 1: Enable
	 */
	uint8_t ucOpChannel;	/* decimal form */
	uint8_t ucOpBand;
	/* 0: Sub 1-GHz
	 * 1: 2.4G
	 * 2: 5G
	 * 3: 6G
	 * 4: 60G
	 */
	uint8_t ucBssid[MAC_ADDR_LEN];
	uint8_t ucTsfOption;
	/* 0: TSF => new TSF = TSF field
	 * 1: TSF offset => new TSF = current TSF - TSF field
	 */
	uint32_t u4Tsf[2];
} __KAL_ATTRIB_PACKED__ __KAL_ATTRIB_ALIGNED__(4);

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
struct LINK g_rPendingAscReqList;

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
const char *asc_status_str(uint8_t i)
{
	/* enum NAN_ASC_STATUS */
	static const char * const status_string[] = {
		[0] = "NAN_ASC_STATUS_SUCCESS",
		[1] = "NAN_ASC_STATUS_SUCCESS_BUT_CHANGED",
		[2] = "NAN_ASC_STATUS_FAIL",
		[3] = "NAN_ASC_STATUS_ASYNC_EVENT",
	};
	if (likely(i < ARRAY_SIZE(status_string)))
		return status_string[i];

	return "Invalid";
}

const char *asc_type_str(uint8_t i)
{
	static const char * const asc_type_string[] = {
		[0] = "Request",
		[1] = "Response",
		[2] = "Confirm",
		[3] = "Reserved",
	};

	if (likely(i < ARRAY_SIZE(asc_type_string)))
		return asc_type_string[i];

	return "Invalid";
}

static void freePendingAscCmd(void)
{
	uint8_t ucIsAscMode;

	ucIsAscMode = g_nanAscPendingCmd.ucIsAscMode;
	DBGLOG(NAN, INFO, "Enter %s, free pending request %u\n",
		   __func__, g_nanAscPendingCmd.ucRequestId);

	kalMemZero(&g_nanAscPendingCmd, sizeof(struct NAN_ASC_PENDING_CMD));

	/* Save current is connected-AP mode or not */
	g_nanAscPendingCmd.ucIsAscMode = ucIsAscMode;
}

static void savePendingAscCmd(struct IE_NAN_ASC_CMD *cmd)
{
	g_nanAscPendingCmd.ucOui = cmd->ucNanOui;
	g_nanAscPendingCmd.ucRequestId = cmd->ucRequestId;
	g_nanAscPendingCmd.type = cmd->type;
	g_nanAscPendingCmd.subtype = cmd->subtype;
	g_nanAscPendingCmd.power_save = cmd->power_save;
	g_nanAscPendingCmd.ucOpChannel = cmd->ucOpChannel;
	g_nanAscPendingCmd.ucOpBand = cmd->ucOpBand;
	g_nanAscPendingCmd.u8Tsf = cmd->u8Tsf;
	g_nanAscPendingCmd.tsf_option = cmd->tsf_option;
	g_nanAscPendingCmd.ucTrackEnable = cmd->track_en;
	g_nanAscPendingCmd.cmd = cmd;

	if (g_nanAscPendingCmd.subtype == NAN_ASC_SUBTYPE_CONNECTED_AP)
		g_nanAscPendingCmd.ucIsAscMode = TRUE;
	else
		g_nanAscPendingCmd.ucIsAscMode = FALSE;

	COPY_MAC_ADDR(g_nanAscPendingCmd.aucBSSID, cmd->aucBSSID);
}

uint32_t nanComposeASCResponse(struct ADAPTER *prAdapter,
				enum NAN_ASC_PACKET_TYPE type,
				enum NAN_ASC_STATUS status,
				enum NAN_ASC_EVENT reason_code,
				struct NAN_ASC_ASYNC_EVENT *prAscAsyncEvt)
{
	uint8_t aucBuf[NAN_MAX_EXT_DATA_SIZE];
	struct IE_NAN_ASC_EVENT *prResponse;
	uint32_t u4BufSize = sizeof(struct IE_NAN_ASC_EVENT) - EXT_MSG_HDR_LEN;

	DBGLOG(NAN, INFO, "Enter %s, type=%u status=%u reason=%u\n",
			   __func__, type, status, reason_code);

	prResponse = (struct IE_NAN_ASC_EVENT *)aucBuf;

	prResponse->ucNanOui = NAN_ASC_CMD_OUI;
	prResponse->u2Length = u4BufSize;
	prResponse->ucMajorVersion = 2;
	prResponse->ucMinorVersion = 1;
	prResponse->ucRequestId = g_nanAscPendingCmd.ucRequestId;
	prResponse->type = type;
	prResponse->status = status;
	prResponse->reason_code= reason_code;

	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
		   prResponse->ucNanOui, prResponse->ucNanOui,
		   oui_str(prResponse->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %d, v%d.%d\n", prResponse->u2Length,
			prResponse->ucMajorVersion, prResponse->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d,Type: %d (%s)\n",
			prResponse->ucRequestId,
			prResponse->type, asc_type_str(prResponse->type));
	DBGLOG(NAN, INFO, "Status: %d (%s),Reason: %d\n",
			prResponse->status, asc_status_str(prResponse->status),
			prResponse->reason_code);

	if (status == NAN_ASC_STATUS_ASYNC_EVENT) {
		prResponse->ucRequestId = 0xFF;

		if (reason_code == NAN_ASC_EVENT_INIT_CLUSTER) {
			struct NAN_ASC_EVENT_CLUSTER_INIT rEvent = {0};

			prResponse->u2Length += sizeof(struct NAN_ASC_EVENT_CLUSTER_INIT);

			kalMemCopy(rEvent.aucAnchorMasterRank,
				prAscAsyncEvt->aucAnchorMasterRank, ANCHOR_MASTER_RANK_NUM);
			COPY_MAC_ADDR(rEvent.aucClusterId, prAscAsyncEvt->ucClusterId);
			COPY_MAC_ADDR(rEvent.aucOwnNMI, prAscAsyncEvt->ucOwnNmi);

			kalMemCopy(prResponse->aucInfo,
				&rEvent,
				sizeof(struct NAN_ASC_EVENT_CLUSTER_INIT));

			DBGLOG(NAN, INFO, "Init cluster\n");
		} else if (reason_code == NAN_ASC_EVENT_JOIN_CLUSTER) {
			struct NAN_ASC_EVENT_CLUSTER_JOIN rEvent = {0};

			prResponse->u2Length += sizeof(struct NAN_ASC_EVENT_CLUSTER_JOIN);

			kalMemCopy(rEvent.aucAnchorMasterRank,
				prAscAsyncEvt->aucAnchorMasterRank, ANCHOR_MASTER_RANK_NUM);
			COPY_MAC_ADDR(rEvent.aucClusterId, prAscAsyncEvt->ucClusterId);
			COPY_MAC_ADDR(rEvent.aucOwnNMI, prAscAsyncEvt->ucOwnNmi);
			COPY_MAC_ADDR(rEvent.aucMasterNMI, prAscAsyncEvt->ucMasterNmi);

			kalMemCopy(&prResponse->aucInfo[0],
				&rEvent,
				sizeof(struct NAN_ASC_EVENT_CLUSTER_JOIN));

			DBGLOG(NAN, INFO, "Join cluster\n");
		} else if (reason_code == NAN_ASC_EVENT_NEW_DEVICE_JOIN) {
			struct NAN_ASC_EVENT_CLUSTER_NEW_DEVICE rEvent = {0};

			prResponse->u2Length += sizeof(struct NAN_ASC_EVENT_CLUSTER_NEW_DEVICE);
			rEvent.aucNewDeviceNMI[0] = 1;
			rEvent.rssi = 2;

			kalMemCopy(&prResponse->aucInfo[0],
				&rEvent,
				sizeof(struct NAN_ASC_EVENT_CLUSTER_NEW_DEVICE));

			DBGLOG(NAN, INFO, "New Device\n");
		} else if (reason_code == NAN_ASC_EVENT_SYNC_BEACON_TRACK) {
			struct NAN_ASC_EVENT_SYNC_BEACON rEvent = {0};

			prResponse->u2Length += sizeof(struct NAN_ASC_EVENT_SYNC_BEACON);
			COPY_MAC_ADDR(rEvent.aucClusterId, prAscAsyncEvt->ucClusterId);
			kalMemCopy(rEvent.aucAnchorMasterRank,
				prAscAsyncEvt->aucAnchorMasterRank, ANCHOR_MASTER_RANK_NUM);
			COPY_MAC_ADDR(rEvent.aucMasterNMI, prAscAsyncEvt->ucMasterNmi);
			/* TODO TSF offset */

			kalMemCopy(&prResponse->aucInfo[0],
				&rEvent,
				sizeof(struct NAN_ASC_EVENT_SYNC_BEACON));
		} else if (reason_code == NAN_ASC_EVENT_DISC_BEACON_TRACK) {
			struct NAN_ASC_EVENT_SYNC_BEACON rEvent = {0};

			prResponse->u2Length += sizeof(struct NAN_ASC_EVENT_DISC_BEACON);
			COPY_MAC_ADDR(rEvent.aucClusterId, prAscAsyncEvt->ucClusterId);
			COPY_MAC_ADDR(rEvent.aucMasterNMI, prAscAsyncEvt->ucMasterNmi);
			/* TODO TSF offset */

			kalMemCopy(&prResponse->aucInfo[0],
				&rEvent,
				sizeof(struct NAN_ASC_EVENT_DISC_BEACON));
		}
		nanExtSendIndication(prAdapter, prResponse, EXT_MSG_SIZE(prResponse));
	} else {
		/* ASYNC event doesn't need to clear ASC CMD */
		kalMemCopy(g_nanAscPendingCmd.cmd, prResponse, EXT_MSG_SIZE(prResponse));
	}

	return WLAN_STATUS_SUCCESS;
}

void nanExtSetAsc(struct ADAPTER *prAdapter)
{
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _NAN_EXT_CMD_ASC_T *prCmd = NULL;

	/* No pending ASC command exist */
	if (g_nanAscPendingCmd.ucOui == 0)
		return;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _NAN_EXT_CMD_ASC_T);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);

	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;
	prTlvCommon->u2TotalElementNum = 0;
	rStatus =
		nicNanAddNewTlvElement(NAN_CMD_EXT_ASC,
			sizeof(struct _NAN_EXT_CMD_ASC_T),
			u4CmdBufferLen, prCmdBuffer);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(TX, ERROR, "Add new Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvElement = nicNanGetTargetTlvElement(1, prCmdBuffer);

	if (prTlvElement == NULL) {
		DBGLOG(TX, ERROR, "Get target Tlv element fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prCmd =
		(struct _NAN_EXT_CMD_ASC_T *)prTlvElement->aucbody;

	prCmd->ucType = g_nanAscPendingCmd.type;
	prCmd->ucSubtype = g_nanAscPendingCmd.subtype;
	prCmd->ucPowerSave = g_nanAscPendingCmd.power_save;
	prCmd->ucOpChannel = g_nanAscPendingCmd.ucOpChannel;
	prCmd->ucOpBand = g_nanAscPendingCmd.ucOpBand;
	COPY_MAC_ADDR(prCmd->ucBssid, g_nanAscPendingCmd.aucBSSID);
	prCmd->u4Tsf[0] = g_nanAscPendingCmd.u8Tsf.u.LowPart;
	prCmd->u4Tsf[1] = g_nanAscPendingCmd.u8Tsf.u.HighPart;
	prCmd->ucTsfOption = g_nanAscPendingCmd.tsf_option;
	prCmd->ucTrackEnable = g_nanAscPendingCmd.ucTrackEnable;

	rStatus = wlanSendSetQueryCmd(prAdapter,
		CMD_ID_NAN_EXT_CMD, TRUE,
		FALSE, FALSE, NULL,
		nicCmdTimeoutCommon, u4CmdBufferLen,
		(uint8_t *)prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);
	freePendingAscCmd();
}

/**
 * nanProcessASC() - Process ASC command from framework
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
uint32_t nanProcessASC(struct ADAPTER *prAdapter, const uint8_t *buf,
		       size_t u4Size)
{
	struct IE_NAN_ASC_CMD *c = (struct IE_NAN_ASC_CMD *)buf;
	uint32_t offset = 0, rStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(NAN, INFO, "Enter %s, consuming %lu bytes\n",
	       __func__, sizeof(struct IE_NAN_ASC_CMD));
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_ASC_CMD));

	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
	       c->ucNanOui, c->ucNanOui, oui_str(c->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %X\n", c->u2Length);
	DBGLOG(NAN, INFO, "v%d.%d\n", c->ucMajorVersion, c->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d\n", c->ucRequestId);
	DBGLOG(NAN, INFO, "Type: %d\n", c->type);
	DBGLOG(NAN, INFO, "SubType: %d\n", c->subtype);
	DBGLOG(NAN, INFO, "PS: %d\n", c->power_save);
	DBGLOG(NAN, INFO, "TrackEnable: %d\n", c->track_en);
	DBGLOG(NAN, INFO, "Rsrv: %d\n", c->reserved);
	DBGLOG(NAN, INFO, "OpChannel: %d\n", c->ucOpChannel);
	DBGLOG(NAN, INFO, "OpBand: %u\n", c->ucOpBand);
	DBGLOG(NAN, INFO, "Rsrv2: %d\n", c->reserved2);
	DBGLOG(NAN, INFO, "bssid: " MACSTR "\n", MAC2STR(c->aucBSSID));
	DBGLOG(NAN, INFO, "TSF: %llu\n", c->u8Tsf);
	DBGLOG(NAN, INFO, "TSFOPT: %u\n", c->tsf_option);
	DBGLOG(NAN, INFO, "Rsrv3: 0x%04X\n", c->reserved3);

	offset += sizeof(struct IE_NAN_ASC_CMD);

	savePendingAscCmd(c);

	/* Direct send response to FWK */
	rStatus = nanComposeASCResponse(prAdapter,
	   NAN_ASC_PACKET_RESPONSE,
	   NAN_ASC_STATUS_SUCCESS,
	   NAN_ASC_EVENT_SUCCESS,
	   NULL);

	/* If NAN is already on, direct send CMD to FW */
	if (g_ucNanIsOn)
		nanExtSetAsc(prAdapter);

	return rStatus;
}

void nanExtComposeClusterEvent(struct ADAPTER *prAdapter, struct NAN_DE_EVENT *prDeEvt)
{
	struct NAN_ASC_ASYNC_EVENT *prAscAsyncEvt;

	prAscAsyncEvt = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
							sizeof(struct NAN_ASC_ASYNC_EVENT));

	if (!prAscAsyncEvt) {
		DBGLOG(NAN, ERROR, "Alloc Async event fail!\n");
		return;
	}
	kalMemZero(prAscAsyncEvt, sizeof(prAscAsyncEvt));

	prAscAsyncEvt->ucEventType = prDeEvt->ucEventType;
	COPY_MAC_ADDR(prAscAsyncEvt->ucClusterId, prDeEvt->ucClusterId);
	kalMemCopy(prAscAsyncEvt->aucAnchorMasterRank,
				prDeEvt->aucAnchorMasterRank, ANCHOR_MASTER_RANK_NUM);
	COPY_MAC_ADDR(prAscAsyncEvt->ucOwnNmi, prDeEvt->ucOwnNmi);

	/* Only JOINED_CLUSTER event will carry Master NMI */
	if (prDeEvt->ucEventType == NAN_EVENT_ID_JOINED_CLUSTER)
		COPY_MAC_ADDR(prAscAsyncEvt->ucMasterNmi,
				prDeEvt->ucMasterNmi);

	nanExtSendAsyncEvent(prAdapter, prAscAsyncEvt);
	cnmMemFree(prAdapter, prAscAsyncEvt);
}
void nanExtComposeBeaconTrack(struct ADAPTER *prAdapter, struct _NAN_EVENT_REPORT_BEACON *prFwEvt)
{
	struct NAN_ASC_ASYNC_EVENT *prAscAsyncEvt;
	struct WLAN_BEACON_FRAME *prWlanBeaconFrame
			= (struct WLAN_BEACON_FRAME *) NULL;

	prWlanBeaconFrame = (struct WLAN_BEACON_FRAME *)
		prFwEvt->aucBeaconFrame;

	prAscAsyncEvt = cnmMemAlloc(prAdapter,
				RAM_TYPE_BUF, sizeof(struct NAN_ASC_ASYNC_EVENT));

	if (!prAscAsyncEvt) {
			DBGLOG(NAN, ERROR, "Alloc Async event fail!\n");
			return;
	}

	kalMemZero(prAscAsyncEvt, sizeof(prAscAsyncEvt));

	if (prWlanBeaconFrame->u2BeaconInterval == 512) {
		DBGLOG(NAN, INFO, "Sync Beacon\n");
		prAscAsyncEvt->ucEventType = NAN_EVENT_ID_SYNC_BEACON_TRACK;
		COPY_MAC_ADDR(prAscAsyncEvt->ucClusterId,
				prWlanBeaconFrame->aucBSSID);
		kalMemCopy(prAscAsyncEvt->aucAnchorMasterRank,
				prFwEvt->aucAnchorMasterRank, ANCHOR_MASTER_RANK_NUM);
		COPY_MAC_ADDR(prAscAsyncEvt->ucMasterNmi,
				prWlanBeaconFrame->aucSrcAddr);
		/* TODO TSF offset */

	} else {
		DBGLOG(NAN, INFO, "Discovery Beacon\n");
		prAscAsyncEvt->ucEventType = NAN_EVENT_ID_DISC_BEACON_TRACK;
		COPY_MAC_ADDR(prAscAsyncEvt->ucClusterId,
				prWlanBeaconFrame->aucBSSID);
		COPY_MAC_ADDR(prAscAsyncEvt->ucMasterNmi,
				prWlanBeaconFrame->aucSrcAddr);
		/* TODO TSF offset */
	}
	nanExtSendAsyncEvent(prAdapter, prAscAsyncEvt);
	cnmMemFree(prAdapter, prAscAsyncEvt);
}

void nanExtSendAsyncEvent(struct ADAPTER *prAdapter, struct NAN_ASC_ASYNC_EVENT *prAscAsyncEvt)
{
	enum NAN_ASC_EVENT reason_code = NAN_ASC_EVENT_FAIL_UNDEFINED;

	if (prAscAsyncEvt->ucEventType == NAN_EVENT_ID_STARTED_CLUSTER) {
		reason_code = NAN_ASC_EVENT_INIT_CLUSTER;
	} else if (prAscAsyncEvt->ucEventType == NAN_EVENT_ID_JOINED_CLUSTER) {
		reason_code = NAN_ASC_EVENT_JOIN_CLUSTER;
	} else if (prAscAsyncEvt->ucEventType == NAN_EVENT_ID_SYNC_BEACON_TRACK) {
		reason_code = NAN_ASC_EVENT_SYNC_BEACON_TRACK;
	} else if (prAscAsyncEvt->ucEventType == NAN_EVENT_ID_DISC_BEACON_TRACK) {
		reason_code = NAN_ASC_EVENT_DISC_BEACON_TRACK;
	} else {
		DBGLOG(NAN, INFO, "undefined Event %d!\n", prAscAsyncEvt->ucEventType);
	}

	if (reason_code != NAN_ASC_EVENT_FAIL_UNDEFINED)
		nanComposeASCResponse(prAdapter,
			   NAN_ASC_PACKET_RESPONSE,
			   NAN_ASC_STATUS_ASYNC_EVENT,
			   reason_code, prAscAsyncEvt);
}

void nanExtTerminateApNan(struct ADAPTER *prAdapter, uint8_t ucReason)
{
		struct IE_NAN_ASC_CMD c = {0};
		uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;

		/* If not in connected-AP mode, no need to back to normal NAN */
		if (!g_ucNanIsOn || !g_nanAscPendingCmd.ucIsAscMode)
			return;

		c.ucNanOui = NAN_ASC_CMD_OUI;
		c.u2Length = sizeof(struct IE_NAN_ASC_CMD) - EXT_MSG_HDR_LEN;
		c.ucMajorVersion = 2;
		c.ucMinorVersion = 1;
		c.ucRequestId = 0xFF;
		c.type = NAN_ASC_PACKET_REQUEST;
		c.subtype = NAN_ASC_SUBTYPE_RETURN_TO_ORIGINAL_NAN;
		c.power_save = 0;
		c.track_en = 0;
		c.ucOpChannel = 0;      /* FW don't care */
		c.ucOpBand = 0; /* FW don't care */
		COPY_MAC_ADDR(c.aucBSSID, aucZeroMacAddr);
		c.u8Tsf.u.LowPart = UINT32_MAX;
		c.u8Tsf.u.HighPart = UINT32_MAX;
		c.tsf_option = 0; /* FW don't care */

		DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
				  c.ucNanOui, c.ucNanOui, oui_str(c.ucNanOui));
		DBGLOG(NAN, INFO, "Length: %X\n", c.u2Length);
		DBGLOG(NAN, INFO, "v%d.%d\n", c.ucMajorVersion, c.ucMinorVersion);
		DBGLOG(NAN, INFO, "ReqId: %d\n", c.ucRequestId);
		DBGLOG(NAN, INFO, "Type: %d\n", c.type);
		DBGLOG(NAN, INFO, "SubType: %d\n", c.subtype);
		DBGLOG(NAN, INFO, "PS: %d\n", c.power_save);
		DBGLOG(NAN, INFO, "TrackEnable: %d\n", c.track_en);
		DBGLOG(NAN, INFO, "Rsrv: %d\n", c.reserved);
		DBGLOG(NAN, INFO, "OpChannel: %d\n", c.ucOpChannel);
		DBGLOG(NAN, INFO, "OpBand: %u\n", c.ucOpBand);
		DBGLOG(NAN, INFO, "Rsrv2: %d\n", c.reserved2);
		DBGLOG(NAN, INFO, "bssid: " MACSTR "\n", MAC2STR(c.aucBSSID));
		DBGLOG(NAN, INFO, "TSF: %llu\n", c.u8Tsf.QuadPart);
		DBGLOG(NAN, INFO, "TSFOPT: %u\n", c.tsf_option);
		DBGLOG(NAN, INFO, "Rsrv3: 0x%04X\n", c.reserved3);

		savePendingAscCmd(&c);
		nanExtSetAsc(prAdapter);
		nanComposeASCResponse(prAdapter,
						  NAN_ASC_PACKET_RESPONSE,
						  NAN_ASC_STATUS_ASYNC_EVENT,
						  ucReason, NULL);
}
#endif
