/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext_ccm.c
 */

/*! \file   "nan_ext_ccm.c"
 *  \brief  This file defines the procedures handling CCM commands.
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


#include "nan_ext_ccm.h"

#if CFG_SUPPORT_NAN_EXT

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_EXT_CMD_CCM_T {
	uint8_t ucCcmEnable;
	uint8_t ucType;
	uint8_t ucStatus;
	uint8_t ucCcmInterval;
	uint16_t u2PassiveScanDuration;
	uint8_t ucCcmSize;
	uint8_t ucCcmPn;
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
static struct _NAN_EXT_CMD_CCM_T g_rNanExtCmdCcm;

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
static void _nanExtCcmCb(struct ADAPTER *prAdapter,
			     struct CMD_INFO *prCmdInfo,
			     uint8_t *pucEventBuf)
{

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return;
	}

	if (prCmdInfo == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prCmdInfo is NULL\n", __func__);
		return;
	}
}

void nanExtSetCcm(struct ADAPTER *prAdapter)
{
	uint32_t rStatus;
	void *prCmdBuffer;
	uint32_t u4CmdBufferLen;
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon = NULL;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	struct _NAN_EXT_CMD_CCM_T *prCmd = NULL;

	u4CmdBufferLen = sizeof(struct _CMD_EVENT_TLV_COMMOM_T) +
			 sizeof(struct _CMD_EVENT_TLV_ELEMENT_T) +
			 sizeof(struct _NAN_EXT_CMD_CCM_T);
	prCmdBuffer = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, u4CmdBufferLen);

	if (!prCmdBuffer) {
		DBGLOG(CNM, ERROR, "Memory allocation fail\n");
		cnmMemFree(prAdapter, prCmdBuffer);
		return;
	}

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prCmdBuffer;
	prTlvCommon->u2TotalElementNum = 0;
	rStatus =
		nicNanAddNewTlvElement(NAN_CMD_EXT_CLUSTER,
			sizeof(struct _NAN_EXT_CMD_CCM_T),
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
		(struct _NAN_EXT_CMD_CCM_T *)prTlvElement->aucbody;

	kalMemCopy(prCmd, &g_rNanExtCmdCcm,
	sizeof(struct _NAN_EXT_CMD_CCM_T));

	rStatus = wlanSendSetQueryCmd(prAdapter,
		CMD_ID_NAN_EXT_CMD, TRUE,
		FALSE, FALSE, _nanExtCcmCb,
		nicCmdTimeoutCommon, u4CmdBufferLen,
		(uint8_t *)prCmdBuffer, NULL, 0);

	cnmMemFree(prAdapter, prCmdBuffer);
}

/**
 * nanProcessCCM() - Process CCM command from framework
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
uint32_t nanProcessCCM(struct ADAPTER *prAdapter, const uint8_t *buf,
		       size_t u4Size)
{
	struct IE_NAN_CCM_CMD *c = (struct IE_NAN_CCM_CMD *)buf;
	uint32_t offset = 0;

	DBGLOG(NAN, INFO, "Enter %s, consuming %zu bytes\n",
	       __func__, sizeof(struct IE_NAN_CCM_CMD));

	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
		c->ucNanOui, c->ucNanOui, oui_str(c->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %d\n", c->u2Length);
	DBGLOG(NAN, INFO, "v%d.%d\n", c->ucMajorVersion, c->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d\n", c->ucRequestId);

	DBGLOG(NAN, INFO, "CCM Enable: %d\n", c->ccm_enable);
	DBGLOG(NAN, INFO, "Type: %d\n", c->cmd_type);
	DBGLOG(NAN, INFO, "Status: %d\n", c->status);
	DBGLOG(NAN, INFO, "CCM Interval: %d\n", c->ucCcmInterval);
	DBGLOG(NAN, INFO, "Passive Scan Duration: %d\n", c->passive_scan_duration);
	DBGLOG(NAN, INFO, "CCM Size: %d\n", c->ccm_size);
	DBGLOG(NAN, INFO, "CCM Participant Number: %d\n", c->ccm_pn);

	g_rNanExtCmdCcm.ucCcmEnable = c->ccm_enable;
	g_rNanExtCmdCcm.ucType = c->cmd_type;
	g_rNanExtCmdCcm.ucStatus = c->status;
	g_rNanExtCmdCcm.ucCcmInterval = c->ucCcmInterval;
	g_rNanExtCmdCcm.u2PassiveScanDuration = c->passive_scan_duration;
	g_rNanExtCmdCcm.ucCcmSize = c->ccm_size;
	g_rNanExtCmdCcm.ucCcmPn = c->ccm_pn;

	nanExtSetCcm(prAdapter);

	offset += sizeof(struct IE_NAN_CCM_CMD);

	return WLAN_STATUS_SUCCESS;
}

#endif
