/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext.c
 */

/*! \file   "nan_ext.c"
 *  \brief  This file defines the interface which can interact with users
 *          in /sys fs.
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

#if CFG_SUPPORT_NAN_EXT

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define NAN_INFO_BUF_SIZE (1024)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

#if 0
#if WLAN_INCLUDE_SYS
extern struct kobject *wifi_kobj;
#else
static struct kobject *wifi_kobj;
#endif

static char acNanInfo[NAN_INFO_BUF_SIZE] = {0};

#endif

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

uint8_t g_ucNanIsOn;

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
const char *oui_str(uint8_t i)
{
	static const char * const oui_string[] = {
		[NAN_CCM_CMD_OUI   - NAN_EXT_CMD_BASE] = "CCM command",
		[NAN_CCM_EVT_OUI   - NAN_EXT_CMD_BASE] = "CCM event",

		[NAN_PA_CMD_OUI    - NAN_EXT_CMD_BASE] = "PA command",
		[NAN_MDC_CMD_OUI   - NAN_EXT_CMD_BASE] = "MDC command",
		[NAN_ASC_CMD_OUI   - NAN_EXT_CMD_BASE] = "ASC command",
		[NAN_AMC_CMD_OUI   - NAN_EXT_CMD_BASE] = "AMC command",
		[NAN_ASCC_CMD_OUI  - NAN_EXT_CMD_BASE] = "AScC command",
		[NAN_FR_EVT_OUI    - NAN_EXT_CMD_BASE] = "Fast Recovery event",
		[NAN_EHT_CMD_OUI   - NAN_EXT_CMD_BASE] = "EHT command",
		[NAN_ADSDC_CMD_OUI - NAN_EXT_CMD_BASE] = "ADSDC command",
		[NAN_ADSDC_TSF_OUI - NAN_EXT_CMD_BASE] = "ADSDC TSF",
	};
	uint8_t ucIndex = i - NAN_EXT_CMD_BASE;

	if  (ucIndex < ARRAY_SIZE(oui_string))
		return oui_string[ucIndex];

	return "Invalid OUI";
}

static uint32_t (* const nanExtCmdHandler[])
	(struct ADAPTER *, const uint8_t *, size_t) = {
	[NAN_CCM_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessCCM,   /* 0x34 */
	[NAN_CCM_EVT_OUI   - NAN_EXT_CMD_BASE] = NULL,		  /* 0x35 */

	[NAN_PA_CMD_OUI    - NAN_EXT_CMD_BASE] = nanProcessPA,    /* 0x37 */
	[NAN_MDC_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessMDC,   /* 0x38 */
	[NAN_ASC_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessASC,   /* 0x39 */
	[NAN_AMC_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessAMC,   /* 0x3A */
	[NAN_ASCC_CMD_OUI  - NAN_EXT_CMD_BASE] = nanProcessASCC,  /* 0x3B */
	[NAN_FR_EVT_OUI    - NAN_EXT_CMD_BASE] = nanProcessFR,    /* 0x3C */
	[NAN_EHT_CMD_OUI   - NAN_EXT_CMD_BASE] = nanProcessEHT,	  /* 0x3D */

	[NAN_ADSDC_CMD_OUI - NAN_EXT_CMD_BASE] = nanProcessADSDC, /* 0x40 */
};

uint32_t nanExtParseCmd(struct ADAPTER *prAdapter,
			uint8_t *buf, uint16_t *u2Size)
{
	uint8_t ucNanOui;
	uint8_t ucCmdByte[NAN_MAX_EXT_DATA_SIZE] = {}; /* working buffer */
	uint32_t rStatus;

	DBGLOG(NAN, INFO, "Cmd Hex:\n");
	DBGLOG_HEX(NAN, INFO, buf, *u2Size)

	/* Both source and destination are in byte array format */
	kalMemCopy(ucCmdByte, buf, kal_min_t(uint16_t, *u2Size,
					     NAN_MAX_EXT_DATA_SIZE));

	ucNanOui = ucCmdByte[0];

	if (ucNanOui < NAN_EXT_CMD_BASE || ucNanOui > NAN_EXT_CMD_END ||
	    nanExtCmdHandler[ucNanOui - NAN_EXT_CMD_BASE] == NULL) {
		DBGLOG(NAN, WARN, "Invalid OUI: %u(0x%X)\n",
		       ucNanOui, ucNanOui);
		return WLAN_STATUS_FAILURE;
	}

	rStatus = nanExtCmdHandler[ucNanOui - NAN_EXT_CMD_BASE](prAdapter,
					      ucCmdByte, sizeof(ucCmdByte));

	*u2Size = kal_min_t(uint16_t,
			    EXT_MSG_SIZE(ucCmdByte), NAN_MAX_EXT_DATA_SIZE);
	kalMemCopy(buf, ucCmdByte, *u2Size);
	DBGLOG(NAN, INFO, "Response Hex:\n");
	DBGLOG_HEX(NAN, INFO, buf, *u2Size);

	return rStatus;
}

void nanExtEnableReq(struct ADAPTER *prAdapter)
{
	g_ucNanIsOn = TRUE;

	nanEhtNanOnHandler(prAdapter);
	nanAdsdcNanOnHandler(prAdapter);
	nanAsccNanOnHandler(prAdapter);
	nanExtSetAsc(prAdapter);
}

void nanExtDisableReq(struct ADAPTER *prAdapter)
{
	nanEhtNanOffHandler(prAdapter);
	nanAdsdcNanOffHandler(prAdapter);
	nanAsccNanOffHandler(prAdapter);
	nanExtTerminateApNan(prAdapter, NAN_ASC_EVENT_ASCC_END_LEGACY);
	g_ucNanIsOn = FALSE;
}

uint32_t nanExtSendIndication(struct ADAPTER *prAdapter,
			      void *buf, uint16_t u2Size)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int r;

	r = mtk_cfg80211_vendor_nan_ext_indication(prAdapter, buf, u2Size);
	if (r != WLAN_STATUS_SUCCESS)
		rStatus = WLAN_STATUS_FAILURE;

	return rStatus;
}
#endif

