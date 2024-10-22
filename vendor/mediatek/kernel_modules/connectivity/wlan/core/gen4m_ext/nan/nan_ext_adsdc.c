/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext_adsdc.c
 */

/*! \file   "nan_ext_adsdc.c"
 *  \brief  This file defines the procedures handling ADSDC commands.
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
#include "nan_ext_adsdc.h"
#include "nan_ext_ascc.h" /* schedule entry */

#if CFG_SUPPORT_NAN_EXT

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define ADSDC_USD_ASCC (1)

#define ADSDC_FW_CHANGED (1)

char aucClearAdsdcHexCmd[] =
{
	0x40, 0x2A, 0x00, 0x02, 0x01,
	0x01, 0x10, 0x01,
	0x00, 0xF0, 0x00, 0x00,
	0x0C,
	0x00, 0x00, 0x00, 0x00,
	0x06, 0x00, 0x18,
	0x0D,
	0x00, 0x00, 0x00, 0x00,
	0x95, 0x00, 0x18,
	0x0E,
	0x00, 0x00, 0x00, 0x00,
	0x06, 0x18, 0x00,
	0x0F,
	0x00, 0x00, 0x00, 0x00,
	0x95, 0x18, 0x00,
	0x03,
};

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_EXT_CMD_USD_T {
	uint8_t ucEnabled;

	uint8_t ucChannelQuality;
	uint8_t ucMasterSelection;
	/* 1: Master */
	uint8_t ucRoleSelection;
	/*
	 * Role Selection:
	 * 0: Publisher solicited and unsolicited SDF
	 * 1: Publisher solicited SDF
	 * 2: Active subscriber
	 * 3: Passive subscriber
	 */
	uint8_t ucServiceId;
	uint8_t ucDiscBcn2g;
	uint8_t ucDiscBcn5g;
	uint8_t ucDiscBcn6g;

	uint32_t u4Category;
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

static struct IE_NAN_ADSDC_PENDING_CMD g_nanAdsdcPendingCmd = {0};

static uint8_t g_nanAdsdcMode = FALSE;

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

static const char *adsdc_type_str(uint8_t i)
{
	static const char * const adsdc_type_string[] = {
		[0] = "Request",
		[1] = "Request (Response not required)",
		[2] = "Response",
		[3] = "Reserved",
	};

	if (likely(i < ARRAY_SIZE(adsdc_type_string)))
		return adsdc_type_string[i];

	return "Invalid";
}

static const char *adsdc_ch_type_str(uint8_t i)
{
	static const char * const adsdc_ch_type_string[] = {
		[0] = "Received signal strength (dBm)",
		[1] = "Quantized signal level",
		[2] = "Signal to noise ratio (dB)",
		[3] = "Reserved",
	};

	if (likely(i < ARRAY_SIZE(adsdc_ch_type_string)))
		return adsdc_ch_type_string[i];

	return "Invalid";
}

static const char *adsdc_role_str(uint8_t i)
{
	static const char * const adsdc_role_string[] = {
		[0] = "Master",
		[1 ... 2] = "Reserved",
		[3] = "Non-master",
	};

	if (likely(i < ARRAY_SIZE(adsdc_role_string)))
		return adsdc_role_string[i];

	return "Invalid";
}

static const char *adsdc_role_type_str(uint8_t i)
{
	static const char * const adsdc_role_type_string[] = {
		[0] = "Publisher solicited and unsolicited SDF",
		[1] = "Publisher solicited SDF",
		[2] = "Active subscriber",
		[3] = "Passive subscriber",
	};

	if (likely(i < ARRAY_SIZE(adsdc_role_type_string)))
		return adsdc_role_type_string[i];

	return "Invalid";
}

static void freePendingAdsdcCmd(struct ADAPTER *prAdapter,
			   struct IE_NAN_ADSDC_PENDING_CMD *prPendingCmd)
{
	DBGLOG(NAN, INFO, "Enter %s, free pending request %u\n",
	       __func__,
	       ((struct IE_NAN_ADSDC_CMD *)(prPendingCmd->cmd))->ucRequestId);

	kalMemZero(prPendingCmd, sizeof(struct IE_NAN_ADSDC_PENDING_CMD));
}

static uint32_t savePendingAdsdcCmd(struct ADAPTER *prAdapter,
		      struct IE_NAN_ADSDC_CMD *cmd)
{
	struct IE_NAN_ADSDC_PENDING_CMD *prPendingCmd = &g_nanAdsdcPendingCmd;

	DBGLOG(NAN, INFO,
	       "req=%u, %u %u %u %u %u c=0x%08x\n",
	       cmd->ucRequestId,
	       cmd->cmd_type,
	       cmd->ch_type,
	       cmd->role,
	       cmd->role_type,
	       cmd->ucServiceId,
	       cmd->u4Category);

	prPendingCmd->ucNanOui = cmd->ucNanOui;

	kalMemCopy(prPendingCmd->cmd, cmd, EXT_MSG_SIZE(cmd));

	return WLAN_STATUS_SUCCESS;
}

uint32_t nanComposeADSDCResponse(struct ADAPTER *prAdapter,
				struct IE_NAN_ADSDC_CMD *cmd,
				enum NAN_ADSDC_PACKET_TYPE type,
				enum NAN_ASCC_STATUS status,
				enum NAN_ADSDC_REASON reason,
				enum NAN_SCHEDULE_METHOD method)
{
	char category_buf[512] = {0};
	struct IE_NAN_ADSDC_EVENT *prResponse;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	u_int8_t update = (status == NAN_ASCC_STATUS_SUCCESS_BUT_CHANGED);

	DBGLOG(NAN, INFO, "Enter %s, request=%u type=%u status=%u reason=%u\n",
	       __func__, cmd->ucRequestId, type, status, reason);

	prResponse = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				 NAN_MAX_EXT_DATA_SIZE);
	if (!prResponse) {
		rStatus = WLAN_STATUS_FAILURE;
		goto done;
	}

	prResponse->ucNanOui = NAN_ADSDC_CMD_OUI;
	if (update) {
		prResponse->u2Length = sizeof(struct IE_NAN_ADSDC_EVENT) -
				ADSDC_EVENT_HDR_LEN +
				ADSDC_CMD_BODY_SIZE(cmd);

		DBGLOG(NAN, TRACE, "u2Length = %u (%zu-%u+%u)\n",
			prResponse->u2Length,
			sizeof(struct IE_NAN_ADSDC_EVENT),
			ADSDC_EVENT_HDR_LEN,
			ADSDC_CMD_BODY_SIZE(cmd));
    } else {
		prResponse->u2Length = 5;
	}

	/* Larger than allocated buffer size */
	if (EXT_MSG_SIZE(prResponse) > NAN_MAX_EXT_DATA_SIZE) {
		rStatus = WLAN_STATUS_FAILURE;
		goto done;
	}

	prResponse->ucMajorVersion = 2;
	prResponse->ucMinorVersion = 1;

	prResponse->ucRequestId = cmd->ucRequestId;
	prResponse->type = type;
	prResponse->status = status;
	prResponse->reason = reason;
	prResponse->scheduling_method = method;

	if (update) {
		prResponse->u4Category = cmd->u4Category;
		kalMemCopy(prResponse->aucEntry, cmd->aucEntry,
			ADSDC_CMD_BODY_SIZE(cmd));
	}

	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
	       prResponse->ucNanOui, prResponse->ucNanOui,
	       oui_str(prResponse->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %d\n", prResponse->u2Length);
	DBGLOG(NAN, INFO, "v%d.%d\n",
	       prResponse->ucMajorVersion, prResponse->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d\n", prResponse->ucRequestId);
	DBGLOG(NAN, INFO, "Type: %d (%s)\n",
	       prResponse->type, adsdc_type_str(prResponse->type));
	DBGLOG(NAN, INFO, "Status: %d (%s)\n",
	       prResponse->status, ascc_status_str(prResponse->status));
	DBGLOG(NAN, INFO, "Reason: %d\n",
	       prResponse->reason);
	DBGLOG(NAN, INFO, "Scheduling method: %d (%s)\n",
	       prResponse->scheduling_method,
	       scheduling_method_str(prResponse->scheduling_method));

	if (!update)
		goto report;

	DBGLOG(NAN, INFO, "Category: 0x%08x (%s)\n", prResponse->u4Category,
	       schedule_category_included_str(prResponse->u4Category,
					      category_buf,
					      sizeof(category_buf)));

	nanParseScheduleEntry(prAdapter, NULL, prResponse->aucEntry,
			      ADSDC_EVENT_BODY_SIZE(prResponse), FALSE);

report:
	/* Reuse the buffer from passed in from HAL */
	DBGLOG(NAN, INFO, "Copy %zu bytes\n", EXT_MSG_SIZE(prResponse));
	kalMemCopy(cmd, prResponse, EXT_MSG_SIZE(prResponse));

done:
	if (prResponse)
		cnmMemFree(prAdapter, prResponse);

	return rStatus;
}

uint32_t nanADSDCResponse(struct ADAPTER *prAdapter,
				struct IE_NAN_ADSDC_CMD *c,
				enum NAN_ADSDC_PACKET_TYPE type,
				enum NAN_ASCC_STATUS status,
				enum NAN_ADSDC_REASON reason,
				enum NAN_SCHEDULE_METHOD method)
{
	return nanComposeADSDCResponse(prAdapter,
			       c,
			       type,
			       status,
			       reason,
			       method);
}

static u_int8_t isNanSocialScheduleEntrySet(uint32_t schedule_category_bitmap)
{

	return !!(schedule_category_bitmap &
		  (BIT(CATEGORY_FC_2G) | BIT(CATEGORY_FC_5G) |
		   BIT(CATEGORY_FC_6G) |
		   BIT(CATEGORY_USD_2G) | BIT(CATEGORY_USD_5G) |
		   BIT(CATEGORY_SD_2G) | BIT(CATEGORY_SD_5G)));
}

static void nanExtSetUsdCommand(
	struct ADAPTER *prAdapter,
	struct IE_NAN_ADSDC_CMD *cmd)
{
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _NAN_EXT_CMD_USD_T *prCmd = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _NAN_EXT_CMD_USD_T);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);

	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;
	prTlvCommon->u2TotalElementNum = 0;
	rStatus =
		nicNanAddNewTlvElement(NAN_CMD_EXT_USD,
			sizeof(struct _NAN_EXT_CMD_USD_T),
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
		(struct _NAN_EXT_CMD_USD_T *)prTlvElement->aucbody;

	prCmd->ucEnabled = 1;
	prCmd->ucChannelQuality = cmd->ch_type;
	prCmd->ucMasterSelection = !cmd->role;
	prCmd->ucRoleSelection = cmd->role_type;
	prCmd->ucServiceId = cmd->ucServiceId;
	prCmd->ucDiscBcn2g = 0;
	prCmd->ucDiscBcn5g = 0;
	prCmd->ucDiscBcn6g = 0;
	prCmd->u4Category = cmd->u4Category;

	rStatus = wlanSendSetQueryCmd(prAdapter,
		CMD_ID_NAN_EXT_CMD, TRUE,
		FALSE, FALSE, NULL,
		nicCmdTimeoutCommon, u4CmdBufferLen,
		(uint8_t *)prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);
}

uint32_t nanProcessAdsdcCommand(
	struct ADAPTER *prAdapter,
	const uint8_t *buf,
	size_t u4Size)
{
	uint32_t r = WLAN_STATUS_SUCCESS;
	char category_buf[512] = {0};
	struct IE_NAN_ADSDC_CMD *c = (struct IE_NAN_ADSDC_CMD *)buf;
	struct TX_DISCOVERY_BEACON *tx_beacon;
	uint32_t offset = 0;

	DBGLOG(NAN, INFO, "Enter %s, consuming %zu bytes\n",
		   __func__, sizeof(struct IE_NAN_ADSDC_CMD));

	/* dumpADSDCCommand, begin */
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_ADSDC_CMD));
	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
		   c->ucNanOui, c->ucNanOui, oui_str(c->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %d\n", c->u2Length);
	DBGLOG(NAN, INFO, "v%d.%d\n", c->ucMajorVersion, c->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d\n", c->ucRequestId);
	DBGLOG(NAN, INFO, "Type: %d (%s)\n",
		   c->cmd_type, adsdc_type_str(c->cmd_type));
	DBGLOG(NAN, INFO, "CH Type: %d\n",
		   c->ch_type, adsdc_ch_type_str(c->ch_type));
	DBGLOG(NAN, INFO, "Role: %d\n", c->role, adsdc_role_str(c->role));
	DBGLOG(NAN, INFO, "Role Type: %d\n",
		   c->role_type, adsdc_role_type_str(c->role_type));
	DBGLOG(NAN, INFO, "ServiceId: %d\n", c->ucServiceId);
	DBGLOG(NAN, INFO, "Category: %08X\n", c->u4Category,
		   schedule_category_included_str(c->u4Category,
						  category_buf,
						  sizeof(category_buf)));
	/* dumpADSDCCommand, end */

	offset += OFFSET_OF(struct IE_NAN_ADSDC_CMD, aucEntry);
	offset += nanParseScheduleEntry(prAdapter, c, c->aucEntry,
				ADSDC_CMD_BODY_SIZE(c),
				TRUE);

	if (ADSDC_CMD_SIZE(c) - offset >= sizeof(struct TX_DISCOVERY_BEACON)) {
		/* After parsing scheduling entries. */
		tx_beacon = (struct TX_DISCOVERY_BEACON *)&buf[offset];

		DBGLOG(NAN, INFO, "tx_discovery_beacon_2g: %u\n",
			   tx_beacon->tx_discovery_beacon_2g);
		DBGLOG(NAN, INFO, "tx_discovery_beacon_5g: %u\n",
			   tx_beacon->tx_discovery_beacon_5g);

		offset += sizeof(struct TX_DISCOVERY_BEACON);
	}
	if (g_ucNanIsOn) {
		DBGLOG(NAN, INFO, "Proceed to send command\n");
		g_nanAdsdcMode = TRUE;
#if ADSDC_USD_ASCC
		/* From ASCC */
		if (true /* c->usage != NAN_SCHEDULE_USAGE_CHECK_SCHEDULE */) {
			/* pass whole command to sync settings */
			nanSchedUpdateCustomCommands(prAdapter, buf,
							 ASCC_CMD_SIZE(buf));
		}
		/* update timeline */
		if (isNanSocialScheduleEntrySet(c->u4Category))
			nanReconfigureCustFaw(prAdapter);
#endif
	} else { /* used later for sending response */
		DBGLOG(NAN, INFO, "Save command to pending list\n");

		/* Enable USD mode first */
		nanExtSetUsdCommand(prAdapter, c);

		r = savePendingAdsdcCmd(prAdapter, c);
		if (r != WLAN_STATUS_SUCCESS)
			return WLAN_STATUS_FAILURE;
	}

	return r;
}

/**
 * nanProcessADSDC() - Process ADSDC command from framework
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
uint32_t nanProcessADSDC(struct ADAPTER *prAdapter, const uint8_t *buf,
			 size_t u4Size)
{
	uint32_t r = WLAN_STATUS_SUCCESS;

	if (prAdapter->rDataPathInfo.ucNDLNum) {
		r = nanADSDCResponse(prAdapter,
			(struct IE_NAN_ADSDC_CMD *)buf,
				 NAN_ADSDC_PACKET_RESPONSE,
				 NAN_ASCC_STATUS_FAIL,
				 NAN_ADSDC_REASON_FAIL_OTHER_USE,
				 NAN_SCHEDULE_METHOD_FW_MANAGED);
		return r;
	}

	r = nanProcessAdsdcCommand(prAdapter, buf, u4Size);
	if (r != WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;

#if ADSDC_FW_CHANGED
	r = nanADSDCResponse(prAdapter, (struct IE_NAN_ADSDC_CMD *)buf,
			     NAN_ADSDC_PACKET_RESPONSE,
			     NAN_ASCC_STATUS_SUCCESS_BUT_CHANGED,
			     NAN_ADSDC_REASON_SUCCESS,
			     NAN_SCHEDULE_METHOD_FRAMEWORK_SUGGESTED_FW);
#else
	r = nanADSDCResponse(prAdapter, (struct IE_NAN_ADSDC_CMD *)buf,
				 NAN_ADSDC_PACKET_RESPONSE,
				 NAN_ASCC_STATUS_SUCCESS,
				 NAN_ADSDC_REASON_SUCCESS,
				 NAN_SCHEDULE_METHOD_FRAMEWORK_FORCED);
#endif

	return r;
}

static void nanAdsdcClearPendingCommands(struct ADAPTER *prAdapter)
{
	struct IE_NAN_ADSDC_PENDING_CMD *prPendingCmd = &g_nanAdsdcPendingCmd;

	freePendingAdsdcCmd(prAdapter, prPendingCmd);
}

static void nanAdsdcProcessAllPendingCommands(struct ADAPTER *prAdapter)
{
	struct IE_NAN_ADSDC_PENDING_CMD *prPendingCmd = &g_nanAdsdcPendingCmd;

	if (prPendingCmd->ucNanOui) {
		DBGLOG(NAN, TRACE, "Process cmd at %p, size=%zu\n",
			prPendingCmd->cmd, EXT_MSG_SIZE(prPendingCmd->cmd));
		nanProcessAdsdcCommand(prAdapter, prPendingCmd->cmd,
			EXT_MSG_SIZE(prPendingCmd->cmd));
		freePendingAdsdcCmd(prAdapter, prPendingCmd);
	}
}

static void nanAdsdcReset(struct ADAPTER *prAdapter)
{
	nanAdsdcClearPendingCommands(prAdapter);
	g_nanAdsdcMode = FALSE;
}

void nanAdsdcNanOffHandler(struct ADAPTER *prAdapter)
{
	nanAdsdcReset(prAdapter);
}

void nanAdsdcNanOnHandler(struct ADAPTER *prAdapter)
{
	nanAdsdcProcessAllPendingCommands(prAdapter);
}

void nanAdsdcBackToNormal(struct ADAPTER *prAdapter)
{
	if (g_nanAdsdcMode) {
		nanProcessAdsdcCommand(prAdapter,
			aucClearAdsdcHexCmd,
			EXT_MSG_SIZE(aucClearAdsdcHexCmd));
		nanAdsdcReset(prAdapter);
	}
}

#endif
