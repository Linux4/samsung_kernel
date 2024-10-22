/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext_pa.c
 */

/*! \file   "nan_ext_pa.c"
 *  \brief  This file defines the procedures handling PA commands.
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


#include "nan_ext_pa.h"

#if CFG_SUPPORT_NAN_EXT

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

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

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/**
 * nanProcessPA() - Process PA command from framework
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
uint32_t nanProcessPA(struct ADAPTER *prAdapter, const uint8_t *buf,
		      size_t u4Size)
{
	struct IE_NAN_PA_CMD *c = (struct IE_NAN_PA_CMD *)buf;
	uint32_t offset = 0;

	DBGLOG(NAN, INFO, "Enter %s, consuming %zu bytes\n",
	       __func__, sizeof(struct IE_NAN_PA_CMD));
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_PA_CMD));

	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
	       c->ucNanOui, c->ucNanOui, oui_str(c->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %d\n", c->ucLength);
	DBGLOG(NAN, INFO, "v%d.%d\n", c->ucMajorVersion, c->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d\n", c->ucRequestId);
	DBGLOG(NAN, INFO, "Type: %d\n", c->type);
	DBGLOG(NAN, INFO, "Status: %d\n", c->status);
	DBGLOG(NAN, INFO, "Minor Reason: %u\n", c->ucMinorReason);
	DBGLOG(NAN, INFO, "Capa: %u\n", c->capability);
	DBGLOG(NAN, INFO, "ServiceReq: %u\n", c->service_request);
	DBGLOG(NAN, INFO, "NdpReq: %u\n", c->ndp_setup_request);
	DBGLOG(NAN, INFO, "Reserved: %u\n", c->reserved);

	offset += sizeof(struct IE_NAN_PA_CMD);

	return WLAN_STATUS_SUCCESS;
}

#endif
