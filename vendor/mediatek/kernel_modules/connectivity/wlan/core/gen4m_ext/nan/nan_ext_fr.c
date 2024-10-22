/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext_fr.c
 */

/*! \file   "nan_ext_fr.c"
 *  \brief  This file defines the procedures handling Fast Recovery commands.
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

#include "nan_ext_fr.h"

#if CFG_SUPPORT_NAN_EXT

static uint32_t nanFrDeviceEntryHandler(struct ADAPTER *prAdapter,
				const struct IE_NAN_FR_DEVICE_ENTRY *fr_device)
{
	DBGLOG(NAN, INFO, "Parsing FR device entry, consuming %zu bytes\n",
	       sizeof(struct IE_NAN_FR_DEVICE_ENTRY));
	DBGLOG_HEX(NAN, INFO, fr_device, sizeof(struct IE_NAN_FR_DEVICE_ENTRY));

	DBGLOG(NAN, INFO, "Device ID: %u\n", fr_device->device_id);
	DBGLOG(NAN, INFO, "reserved: %u\n", fr_device->reserved);
	DBGLOG(NAN, INFO, "Peer NMI: " MACSTR "\n",
	       MAC2STR(fr_device->aucPeerNMIAddr));

	return sizeof(struct IE_NAN_FR_DEVICE_ENTRY);
}

static uint32_t nanParseFrDeviceEntry(struct ADAPTER *prAdapter,
			      const struct IE_NAN_FR_DEVICE_ENTRY *fr_device,
			      uint32_t size)
{
	uint32_t offset = 0;

	while (offset + sizeof(struct IE_NAN_FR_DEVICE_ENTRY) <= size) {
		DBGLOG(NAN, INFO,
		       "Parsing FR device id %u, remaining: %u\n",
		       fr_device->device_id, size - offset);

		offset += nanFrDeviceEntryHandler(prAdapter, fr_device);
		fr_device++;
	}

	return offset;
}

static const char *fr_type_str(uint8_t i)
{
	/* enum NAN_FR_PACKET_TYPE */
	static const char * const fr_type_string[] = {
		[0] = "Request",
		[1] = "Response",
		[2] = "Reserved",
		[3] = "Reserved",
	};

	if (likely(i < ARRAY_SIZE(fr_type_string)))
		return fr_type_string[i];

	return "Invalid";
}

static const char *fr_subtype_str(uint8_t type, uint8_t subtype)
{
	/* enum NAN_FR_PACKET_SUBTYPE */
	static const char *subtype_string[][4] = {
		{ /* type 0 (Request) */
			[0] = "Remove the devices from list",
			[1] = "Remove all",
			[2] = "Check device list",
			[3] = "Reserved",
		},
		{ /* type 1 (Response) */
			[0] = "Success",
			[1] = "Fail",
			[2] = "Asynchronous event",
			[3] = "Reserved",
		},
	};

	if (type < ARRAY_SIZE(subtype_string) &&
	    subtype < ARRAY_SIZE(subtype_string[0]))
		return subtype_string[type][subtype];

	return "Invalid";
}

/**
 * nanProcessFR() - Process FR command from framework
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
uint32_t nanProcessFR(struct ADAPTER *prAdapter, const uint8_t *buf,
		      size_t u4Size)
{
	struct IE_NAN_FR_EVENT *e = (struct IE_NAN_FR_EVENT *)buf;
	uint32_t offset = 0;

	DBGLOG(NAN, INFO, "Enter %s, consuming %zu bytes\n",
	       __func__, sizeof(struct IE_NAN_FR_EVENT));
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_FR_EVENT));

	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
		e->ucNanOui, e->ucNanOui, oui_str(e->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %d\n", e->u2Length);
	DBGLOG(NAN, INFO, "v%d.%d\n", e->ucMajorVersion, e->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d\n", e->ucRequestId);
	DBGLOG(NAN, INFO, "Type: %u (%s)\n", e->type, fr_type_str(e->type));
	DBGLOG(NAN, INFO, "SubType: %u (%s)\n",
		e->subtype, fr_subtype_str(e->type, e->subtype));
	DBGLOG(NAN, INFO, "DevNum: %u\n", e->ucNumDev);

	if (sizeof(struct IE_NAN_FR_DEVICE_ENTRY) * e->ucNumDev >
	    FR_EVENT_BODY_SIZE(e)) {
		DBGLOG(NAN, WARN, "DevNum too large: %lu * %u > %lu\n",
		       sizeof(struct IE_NAN_FR_DEVICE_ENTRY) * e->ucNumDev,
		       e->ucNumDev, FR_EVENT_BODY_SIZE(e));
	}
	offset += OFFSET_OF(struct IE_NAN_FR_EVENT, arDevice);
	offset += nanParseFrDeviceEntry(prAdapter, e->arDevice,
					FR_EVENT_BODY_SIZE(e));

	return WLAN_STATUS_SUCCESS;
}

static void dumpFrResponse(struct IE_NAN_FR_EVENT *r)
{
	uint32_t i;

	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
	       r->ucNanOui, r->ucNanOui, oui_str(r->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %d\n", r->u2Length);
	DBGLOG(NAN, INFO, "v%d.%d\n", r->ucMajorVersion, r->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d\n", r->ucRequestId);
	DBGLOG(NAN, INFO, "Type: %d (%s)\n",
	       r->type, fr_type_str(r->type));
	DBGLOG(NAN, INFO, "Subtype: %d (%s)\n",
	       r->subtype, fr_subtype_str(r->type, r->subtype));
	DBGLOG(NAN, INFO, "NumDev: %d\n", r->ucNumDev);

	for (i = 0; i < r->ucNumDev; i++) {
		DBGLOG(NAN, INFO, "device_id: %d, PeerNMI: " MACSTR "\n",
		       r->arDevice[i].device_id,
		       MAC2STR(r->arDevice[i].aucPeerNMIAddr));
	}
}

uint32_t nanIndicateFrDeleted(struct ADAPTER *prAdapter,
			      struct _NAN_NDL_INSTANCE_T *prNDL)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t buf[NAN_MAX_EXT_DATA_SIZE];
	struct IE_NAN_FR_EVENT *prResponse = (struct IE_NAN_FR_EVENT *)buf;

	prResponse->ucNanOui = NAN_FR_EVT_OUI;
	prResponse->u2Length =
		sizeof(struct IE_NAN_FR_EVENT) - FR_EVENT_HDR_LEN +
		sizeof(struct IE_NAN_FR_DEVICE_ENTRY[1]);
	prResponse->ucMajorVersion = 2;
	prResponse->ucMinorVersion = 1;
	prResponse->ucRequestId = prNDL->u4FastRecoveryId;
	prResponse->type = NAN_FR_PACKET_RESPONSE;
	prResponse->subtype = NAN_FR_RESPONSE_ASYNC;
	prResponse->ucNumDev = 1;
	prResponse->arDevice[0].device_id = prNDL->ucIndex;
	COPY_MAC_ADDR(prResponse->arDevice[0].aucPeerNMIAddr,
		      prNDL->aucPeerMacAddr);

	nanExtSendIndication(prAdapter, prResponse, EXT_MSG_SIZE(prResponse));
	return rStatus;
}

uint32_t nanIndicateActiveFrResponse(struct ADAPTER *prAdapter,
				uint8_t ucRequestId,
				enum NAN_FR_PACKET_TYPE type,
				enum NAN_FR_PACKET_SUBTYPE subtype)
{
	uint8_t buf[NAN_MAX_EXT_DATA_SIZE];
	struct IE_NAN_FR_EVENT *prResponse;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t *pucNumDev;
	uint16_t *pu2Length;
	uint8_t ucNumNDL;
	uint32_t u4NdlIdx;

	DBGLOG(NAN, INFO, "Enter %s, request=%u type=%u subtype=%u\n",
	       __func__, ucRequestId, type, subtype);

	/* Get a buffer to fill response */
	prResponse = (struct IE_NAN_FR_EVENT *)buf;

	prResponse->ucNanOui = NAN_FR_EVT_OUI;
	prResponse->u2Length =
		sizeof(struct IE_NAN_FR_EVENT) - FR_EVENT_HDR_LEN;
	DBGLOG(NAN, TRACE, "u2Length = %u (%zu-%lu)\n",
	       prResponse->u2Length,
	       sizeof(struct IE_NAN_FR_EVENT), FR_EVENT_HDR_LEN);
	/* Larger than allocated buffer size */
	if (EXT_MSG_SIZE(prResponse) > NAN_MAX_EXT_DATA_SIZE) {
		rStatus = WLAN_STATUS_FAILURE;
		goto done;
	}

	prResponse->ucMajorVersion = 2;
	prResponse->ucMinorVersion = 1;

	prResponse->ucRequestId = ucRequestId;
	prResponse->type = type;
	prResponse->subtype = subtype;
	prResponse->ucNumDev = 0;
	pucNumDev = &prResponse->ucNumDev;
	pu2Length = &prResponse->u2Length;
	ucNumNDL = kal_min_t(uint8_t, NAN_MAX_SUPPORT_NDL_NUM,
			     prAdapter->rWifiVar.ucNanMaxNdpSession);

	for (u4NdlIdx = 0; u4NdlIdx < ucNumNDL; u4NdlIdx++) {
		struct _NAN_NDL_INSTANCE_T *prNDL;

		prNDL = &prAdapter->rDataPathInfo.arNDL[u4NdlIdx];
		if (!prNDL->fgNDLValid || !prNDL->u4SetFastRecovery == 0)
			continue;

		DBGLOG(NAN, INFO, "Add #%d FR schedule record %d\n",
		       *pucNumDev, u4NdlIdx);
		prResponse->arDevice[*pucNumDev].device_id = prNDL->ucIndex;
		COPY_MAC_ADDR(prResponse->arDevice[*pucNumDev].aucPeerNMIAddr,
			      prNDL->aucPeerMacAddr);
		(*pucNumDev)++;
		*pu2Length += sizeof(struct IE_NAN_FR_DEVICE_ENTRY);
	}

	/* log to verify the result */
	dumpFrResponse(prResponse);

	/* Reuse the buffer from passed in from HAL */
	DBGLOG(NAN, INFO, "Copy %zu bytes\n", EXT_MSG_SIZE(prResponse));

	nanExtSendIndication(prAdapter, prResponse, EXT_MSG_SIZE(prResponse));

done:
	return rStatus;
}

#endif
