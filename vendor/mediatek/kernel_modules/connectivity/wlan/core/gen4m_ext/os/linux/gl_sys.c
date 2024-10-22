/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /os/linux/gl_sys.c
 */

/*! \file   "gl_sys.c"
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
#include "debug.h"
#include "fw_dl.h"
#include "gl_fw_log.h"
#include "gl_rst.h"
#include "wlan_lib.h"
#include "debug.h"
#include "wlan_oid.h"
#include <linux/rtc.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include "gl_coredump.h"


#if WLAN_INCLUDE_SYS

#define SYS_FW_LOG_SUPPORT 1

/* Project Definition. */
#if CFG_SUPPORT_CONNAC3X
#if CFG_SUPPORT_WIFI_6G
#define CFG_FS_WIFI6E_MIMO 1
#endif
#endif
#if CFG_SUPPORT_802_11AX
#define CFG_FS_WIFI6_MIMO 1
#endif
#if CFG_SUPPORT_CONNAC2X_2x2
#define CFG_FS_WIFI5_MIMO 1
#endif

#if SYS_FW_LOG_SUPPORT
#define FW_LOG_WIFI_WTF_INF_NAME "fw_log_wifi_write_log_to_file"
#endif

extern uint8_t *apucRstReason[RST_REASON_MAX];
#if IS_ENABLED(CFG_MTK_WIFI_CONNV3_SUPPORT)
extern void connv3_coredump_set_memdump_mode(unsigned int mode);
#endif
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define MTK_INFO_MAX_SIZE 256

#define FS_VERSION				9
#define FS_VERSION_SIZE				2

#define FS_SOLUTION_PROVIDER_SIZE		3

/* HW Feature Set */
#define FS_HW_FEATURE_SIZE			2
#define FS_HW_FEATURE_LEN			4

#define FS_HW_STANDARD_WIFI4			0
#define FS_HW_STANDARD_WIFI5			1
#define FS_HW_STANDARD_WIFI6			2
#define FS_HW_STANDARD_WIFI6E			3
#define FS_HW_STANDARD_WIFI7			4

#define FS_HW_LP_RX_CORE_OFFSET			4
#define FS_HW_LP_RX_CORE			1

#define FS_HW_NUM_CORES_OFFSET			5
#define FS_HW_NUM_CORES_ONE			1
#define FS_HW_NUM_CORES_TWO			2

#define FS_HW_CONCURRENCY_MODE_OFFSET		8
#define FS_HW_CONCURRENCY_MODE_ONE		1
#define FS_HW_CONCURRENCY_MODE_TWO		2

#define FS_HW_NUM_ANT_OFFSET			11
#define FS_HW_NUM_ANT_SISO			1
#define FS_HW_NUM_ANT_MIMO			2

/* SW Feature Set */
#define FS_SW_FEATURE_NUM			16
#define FS_SW_FEATURE_LEN			132
#define FS_SW_MAX_DATA_LEN			8

/* 1 PNO */
#define FS_SW_PNO_ID				1
#define FS_SW_PNO_LEN				1
#define FS_SW_PNO_SUPPORT			BIT(0)
#define FS_SW_PNO_UNASSOC			BIT(1)
#define FS_SW_PNO_ASSOC				BIT(2)

/* 2 TWT  */
#define FS_SW_TWT_ID				2
#define FS_SW_TWT_LEN				2
#define FS_SW_TWT_SUPPORT			BIT(0)
#define FS_SW_TWT_REQUESTER			BIT(1)
#define FS_SW_TWT_BROADCAST			BIT(2)
#define FS_SW_TWT_FLEXIBLE			BIT(3)
/* B4~B5: Min Service Period (0: 4ms, 1:8ms, 2:12ms, 3:16ms) */
#define FS_SW_TWT_MIN_SERVICE			3
#define FS_SW_TWT_MIN_SERVICE_OFFSET		4
/* B6~B7: Min Sleep Period   (0: 8ms, 1:16ms, 2:24ms, 3:32ms) */
#define FS_SW_TWT_MIN_SLEEP			3
#define FS_SW_TWT_MIN_SLEEP_OFFSET		6

/* 3 Wi-Fi Optimizer */
#define FS_SW_OPTI_ID				3
#define FS_SW_OPTI_LEN				1
#define FS_SW_OPTI_SUPPORT			BIT(0)
/* B1: Dynamic scan dwell control */
#define FS_SW_OPTI_B1				BIT(1)
/* B2: Enhanced passive scan */
#define FS_SW_OPTI_B2				BIT(2)

/* 4 Scheduled PM */
#define FS_SW_SCHE_PM_ID			4
#define FS_SW_SCHE_PM_LEN			1
#define FS_SW_SCHE_PM_SUPPORT			BIT(0)
/* B1~B2: Min Service Period (0: 4ms, 1:8ms, 2:12ms, 3:16ms) */
#define FS_SW_SCHE_PM_MIN_SERVICE		0
#define FS_SW_SCHE_PM_MIN_SERVICE_OFFSET	1
/* B3~B4: Min Sleep Period   (0: 8ms, 1:16ms, 2:24ms, 3:32ms) */
#define FS_SW_SCHE_PM_MIN_SLEEP			0
#define FS_SW_SCHE_PM_MIN_SLEEP_OFFSET		3

/* 5 Delayed Wakeup */
#define FS_SW_D_WAKEUP_ID			5
#define FS_SW_D_WAKEUP_LEN			1
#define FS_SW_D_WAKEUP_SUPPORT			BIT(0)

/* 6 RFC8325 */
#define FS_SW_RFC8325_ID			6
#define FS_SW_RFC8325_LEN			1
#define FS_SW_RFC8325_SUPPORT			BIT(0)

/* 7 MHS */
/* 1st byte */
#define FS_SW_MHS_GET_VALID_CH			BIT(0)
#define FS_SW_MHS_11AX				BIT(1)
#define FS_SW_MHS_WPA3				BIT(2)

/* 2nd byte */
#define FS_SW_MHS_ID				7
#define FS_SW_MHS_LEN				2
#define FS_SW_MHS_DUAL_INF			BIT(0) /* B0 */
#define FS_SW_MHS_5G				BIT(1)
#define FS_SW_MHS_6G				BIT(2)
#define FS_SW_MHS_MAX_CLIENT			15 /* B3~B6 MAX Clients */
#define FS_SW_MHS_MAX_CLIENT_OFFSET		3
#define FS_SW_MHS_CC_HAL			BIT(7)

/* 8 Roaming */
#define FS_SW_ROAM_ID				8
#define FS_SW_ROAM_LEN				4
#define FS_SW_ROAM_VER_MAJOR			4 /* 1st byte */
#define FS_SW_ROAM_VER_MINOR			0 /* 2nd byte */

/* 3rd byte */
/* B0: High Channel Utilization Trigger */
#define FS_SW_ROAM_3B_B0			BIT(0)
/* B1: Emergence Roaming Trigger */
#define FS_SW_ROAM_3B_B1			BIT(1)
/* B2: BTM Roaming Trigger */
#define FS_SW_ROAM_3B_B2			BIT(2)
/* B3: Idle Roaming Trigger */
#define FS_SW_ROAM_3B_B3			BIT(3)
/* B4: WTC Roaming Trigger */
#define FS_SW_ROAM_3B_B4			BIT(4)
/* B5: BT Coex Roaming Trigger */
#define FS_SW_ROAM_3B_B5			BIT(5)
/* B6: Roaming between WPA and WPA2 Roaming */
#define FS_SW_ROAM_3B_B6			BIT(6)
/* B7: Manage Channel List API */
#define FS_SW_ROAM_3B_B7			BIT(7)

/* 4th byte */
/* B0: Adaptive 11r */
#define FS_SW_ROAM_4B_B0			BIT(0)
/* B1: Roaming Control API - 1.GET, 2.SET Roam Trigger */
#define FS_SW_ROAM_4B_B1			BIT(1)
/* B2: Roaming Control API - 3/4.Reassoc */
#define FS_SW_ROAM_4B_B2			BIT(2)
/* B3: Roaming Control API - 5.GET CU */
#define FS_SW_ROAM_4B_B3			BIT(3)

/* 9 NCHO */
#define FS_SW_NCHO_ID				9
#define FS_SW_NCHO_LEN				2
#define FS_SW_NCHO_VER_MAJOR			3
#define FS_SW_NCHO_VER_MINOR			5

/* 10 ASSURANCE */
#define FS_SW_ASSUR_ID				10
#define FS_SW_ASSUR_LEN				1
#define FS_SW_ASSUR_SUPPORT			BIT(0)

/* 11 Frame Pcap Logging */
#define FS_SW_FRAMEPCAP_ID			11
#define FS_SW_FRAMEPCAP_LEN			1
#define FS_SW_FRAMEPCAP_LOG			BIT(0)

/* 12 Security */
#define FS_SW_SEC_ID				12
#define FS_SW_SEC_LEN				2
#define FS_SW_SEC_WPA3_SAE_HASH			BIT(0)
#define FS_SW_SEC_WPA3_SAE_FT			BIT(1)
#define FS_SW_SEC_WPA3_ENT_S_B			BIT(2)
#define FS_SW_SEC_WPA3_ENT_S_B_192		BIT(3)
#define FS_SW_SEC_FILS_SHA256			BIT(4)
#define FS_SW_SEC_FILS_SHA384			BIT(5)
#define FS_SW_SEC_FILS_SHA256_FT		BIT(6)
#define FS_SW_SEC_FILS_SHA384_FT		BIT(7)

/* 2nd byte */
#define FS_SW_SEC_ENHANCED_OPEN			BIT(0)

/* 13 P2P */
#define FS_SW_P2P_ID				13
#define FS_SW_P2P_LEN				6

/* 1st byte */
#define FS_SW_P2P_NAN				BIT(0)
#define FS_SW_P2P_TDLS				BIT(1)
#define FS_SW_P2P_P2P6E				BIT(2)
#define FS_SW_P2P_LISTEN_OFFLOAD		BIT(3)
#define FS_SW_P2P_NOA_PS			BIT(4)
#define FS_SW_P2P_TDLS_OFF_CH			BIT(5)
#define FS_SW_P2P_TDLS_CAP_ENHANCE		BIT(6)
#define FS_SW_P2P_TDLS_PS			BIT(7)

/* 2nd byte
 * B0~B3 : Number of max TDLS connections
 * B4~B7 : Number of max NAN NDP
 */
#define FS_SW_P2P_TDLS_MAX_NUM			4
#define FS_SW_P2P_NAN_NDP_MAX_WIFI7		8
#define FS_SW_P2P_NAN_NDP_MAX_WIFI6E		8
#define FS_SW_P2P_NAN_NDP_MAX_OFFSET		4

/* 3rd byte */
#define FS_SW_P2P_STA_P2P			BIT(0)
#define FS_SW_P2P_STA_SAP			BIT(1)
#define FS_SW_P2P_STA_NAN			BIT(2)
#define FS_SW_P2P_STA_TDLS			BIT(3)
#define FS_SW_P2P_STA_SAP_P2P			BIT(4)
#define FS_SW_P2P_STA_SAP_NAN			BIT(5)
#define FS_SW_P2P_STA_P2P_P2P			BIT(6)
#define FS_SW_P2P_STA_P2P_NAN			BIT(7)

/* 4th byte */
#define FS_SW_P2P_STA_P2P_TDLS			BIT(0)
#define FS_SW_P2P_STA_SAP_TDLS			BIT(1)
#define FS_SW_P2P_STA_NAN_TDLS			BIT(2)
#define FS_SW_P2P_STA_SAP_P2P_TDLS		BIT(3)
#define FS_SW_P2P_STA_SAP_NAN_TDLS		BIT(4)
#define FS_SW_P2P_STA_P2P_P2P_TDLS		BIT(5)
#define FS_SW_P2P_STA_P2P_NAN_TDLS		BIT(6)

/* 6th byte */
#define FS_SW_P2P_NAN6E_STD			BIT(0)
#define FS_SW_P2P_NAN6E_SS			BIT(1)
#define FS_SW_P2P_NAN_FC			BIT(2)
#define FS_SW_P2P_NAN_REL_VER			3
#define FS_SW_P2P_NAN_REL_VER_OFFSET		3

/* 14 BIG DATA */
#define FS_SW_BIGD_ID				14
#define FS_SW_BIGD_LEN				1
#define FS_SW_BIGD_GETBSSINFO			BIT(0)
#define FS_SW_BIGD_GETASSOCREJECTINFO		BIT(1)
#define FS_SW_BIGD_GETSTAINFO			BIT(2)

/* 15 GET STA DUMP */
#define FS_SW_GETSTADUMP_ID			15
#define FS_SW_GETSTADUMP_LEN			1
#define FS_SW_GETSTADUMP_SUPPORT		BIT(0)

/* 16 STD+ */
#define FS_SW_STDPLUS_ID			16
#define FS_SW_STDPLUS_LEN			7
/* 1st byte */
#define FS_SW_STDPLUS_VER_MAJOR			2
/* 2nd byte */
#define FS_SW_STDPLUS_VER_MINOR			1
/* 3rd byte */
/* Reserve */
/* 4th byte */
/* Reserve */
/* 5th byte */
#define FS_SW_STDPLUS_ESDF		BIT(0)
#define FS_SW_STDPLUS_FESDF		BIT(1)
/* 6th byte */
#define FS_SW_STDPLUS_PANAN		BIT(0)
#define FS_SW_STDPLUS_FC		BIT(1)
#define FS_SW_STDPLUS_FR		BIT(2)
#define FS_SW_STDPLUS_ANS		BIT(3)
#define FS_SW_STDPLUS_EHT4		BIT(4)
#define FS_SW_STDPLUS_EHT3		BIT(5)
#define FS_SW_STDPLUS_EHT2		BIT(6)
#define FS_SW_STDPLUS_EHT1		BIT(7)
/* 7th byte */
#define FS_SW_STDPLUS_ASC		BIT(0)
#define FS_SW_STDPLUS_AMC		BIT(1)
#define FS_SW_STDPLUS_ASCC		BIT(2)
#define FS_SW_STDPLUS_ADSDC		BIT(3)
#define FS_SW_STDPLUS_PAIRING		BIT(4)
#define FS_SW_STDPLUS_APNAN		BIT(5)
#define FS_SW_STDPLUS_CCM		BIT(6)
#define FS_SW_STDPLUS_MDC		BIT(7)


#define MANIFEST_BUFFER_SIZE	256

#define RST_REPORT_DATA_MAX_LEN 512

#ifndef INI_ARGV_MAX
#define INI_ARGV_MAX 8
#endif
#ifndef MAX_INI_KEY_LEN
#define MAX_INI_KEY_LEN	64
#endif
#ifndef MAX_READ_ENTRY
#define MAX_READ_ENTRY	128
#endif
#ifndef MAX_TABLE_ENTRY
#define MAX_TABLE_ENTRY	128
#endif

struct INI_PARSE_STATE_S {
	int8_t *ptr;
	int8_t *text;
	uint32_t textsize;
	int32_t nexttoken;
	uint32_t maxSize;
};

enum {
	INI_STATE_EOF 		= 0,
	INI_STATE_TEXT 		= 1,
	INI_STATE_NEWLINE 	= 2,
	INI_STATE_ERROR 	= 3
};

struct INI_TABLE_INFO {
	uint8_t u8Key[MAX_INI_KEY_LEN];
	int32_t i8Default;
	int32_t i8Min;
	int32_t i8Max;
	uint8_t fgHasRange;
};

struct INI_READ_INFO {
	uint8_t u8Key[MAX_INI_KEY_LEN];
	int32_t i8Value;
};

#ifndef CFG_SUPPORT_CABLE_DETECT
#define CFG_SUPPORT_CABLE_DETECT 0
#endif

#if CFG_SUPPORT_CABLE_DETECT
#define CABLE_STATUS_UNKNOWN 0
#define CABLE_STATUS_PLUG_IN 1
#define CABLE_STATUS_PULL_OUT 2
#endif

#if CFG_HDM_WIFI_SUPPORT
static int32_t g_hdm_wlan_loader = 0;
#endif

#define WIFI7_CFG_FILE_PATH "/vendor/firmware/wifi7.cfg"
static u_int8_t g_IsWifi7CfgFile = FALSE;

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

#if SYS_FW_LOG_SUPPORT
struct fw_log_wifi_WtF_interface {
	struct cdev cdev;
	dev_t devno;
	struct class *driver_class;
	struct device *class_dev;
	int32_t fgIsAllowFWLogDump;
	wait_queue_head_t wq;
};

struct fw_log_wifi_WtF_interface fw_log_wifi_WtF_inf;

static unsigned int fw_log_wifi_WtF_poll(struct file *filp, poll_table *wait);

const struct file_operations fw_log_wifi_WtF_fops = {
	.poll = fw_log_wifi_WtF_poll,
};

#endif

/* SW Feature Set ID/Len/Data */
struct FS_SW_ILD_T {
	u_int8_t u8ID;
	u_int8_t u8Len;
	u_int8_t aucData[FS_SW_MAX_DATA_LEN];
};

struct PARAM_HANG_INFO {
	uint8_t id;
	uint8_t len;
	uint8_t fwVer[30];
	uint8_t driverVer[30];
	uint8_t cidInfo[30];
	uint32_t hangType;
	uint8_t rawData[512];
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

static struct GLUE_INFO *g_prGlueInfo;
static struct kobject *wifi_kobj;
static uint8_t aucMacAddrOverride[] = "FF:FF:FF:FF:FF:FF";
#if defined(CFG_MOUTON)
static uint8_t aucDefaultFWVersion[] = "MOUTON_DEFAULT";
#elif defined(CFG_TALBOT)
static uint8_t aucDefaultFWVersion[] = "TALBOT_DEFAULT";
#elif defined(CFG_CERVENO)
static uint8_t aucDefaultFWVersion[] = "CERVINO_DEFAULT";
#else
static uint8_t aucDefaultFWVersion[] = "Unknown";
#endif
static u_int8_t fgIsMacAddrOverride = FALSE;
static int32_t g_i4PM = -1;
static int32_t g_i4Ant = -1;
static char acVerInfo[MTK_INFO_MAX_SIZE];
static char acSoftAPInfo[MTK_INFO_MAX_SIZE];
#if SYS_FW_LOG_SUPPORT
static int32_t i4DumpInProgress;
#endif

static char acFeatureInfo[MTK_INFO_MAX_SIZE];
static uint8_t aucSolutionProvider[] = "MTK";
#if CFG_SUPPORT_CABLE_DETECT
static u_int8_t aucMainCableDetectStatus = CABLE_STATUS_UNKNOWN;
int32_t g_i4MainCableDetectGpio = -1;
static u_int8_t aucSubCableDetectStatus = CABLE_STATUS_UNKNOWN;
int32_t g_i4SubCableDetectGpio = -1;
uint8_t g_CableInfo[8] = {0};
#endif

uint8_t g_wifiVer[MANIFEST_BUFFER_SIZE] = {0};
uint32_t g_wifiVer_length;

#if BUILD_QA_DBG
uint32_t g_u4Memdump = 3;
#else
uint32_t g_u4Memdump = 2;
#endif

uint8_t *g_pucTraces;

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void iniFileErrorCheck (struct ADAPTER *prAdapter,
	uint8_t **ppucIniBuf, uint32_t *pu4ReadSize);
void iniTableParsing(uint32_t *);
uint32_t iniFileParsing(uint8_t *, uint32_t);
int32_t iniFindNextToken(struct INI_PARSE_STATE_S *state);

uint32_t rsnSetCountryCodefor6g(
	struct ADAPTER *prAdapter,
	uint8_t mode)
{
	uint8_t country[2] = {0};
	uint32_t u4BufLen;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	/* set country code to NULL for all 6G channel */
	if (mode == 1) {
		country[0] = 0;
		country[1] = 0;
		/* save current country code for restore */
		prAdapter->u2CurCountryCode =
			prAdapter->rWifiVar.u2CountryCode;
		DBGLOG(RSN, INFO,
			"Set country code NULL, current country code: %c%c\n",
			(prAdapter->u2CurCountryCode & 0xff00) >> 8,
			prAdapter->u2CurCountryCode & 0x00ff);

		rStatus = kalIoctl(prAdapter->prGlueInfo,
			wlanoidSetCountryCode,
			country, 2, &u4BufLen);
	} else if (mode == 0) {
		/* restore current country code */
		if (prAdapter->u2CurCountryCode != 0) {
			country[0] = (prAdapter->u2CurCountryCode
					& 0xff00) >> 8;
			country[1] = prAdapter->u2CurCountryCode
					& 0x00ff;
			DBGLOG(RSN, INFO,
				"Set country code back: %c%c\n",
				country[0], country[1]);

			rStatus = kalIoctl(
				prAdapter->prGlueInfo,
				wlanoidSetCountryCode,
				country, 2, &u4BufLen);
		}
	}

	return rStatus;
}

static ssize_t pm_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(g_i4PM),
		"%d", g_i4PM);
}

static void pm_EnterCtiaMode(void)
{
	if (!g_prGlueInfo)
		DBGLOG(INIT, ERROR, "g_prGlueInfo is null\n");
	else if (g_i4PM == -1)
		DBGLOG(INIT, TRACE, "keep default\n");
	else {
		g_prGlueInfo->prAdapter->fgEnDbgPowerMode = !g_i4PM;
		nicEnterCtiaMode(g_prGlueInfo->prAdapter,
			!g_i4PM,
			FALSE);
		/* .psm.info = g_i4PM */
#if defined(CFG_FS_WIFI6E_MIMO)
		g_prGlueInfo->prAdapter->fgEnRfTestMode = !g_i4PM;
		rsnSetCountryCodefor6g(g_prGlueInfo->prAdapter,
			!g_i4PM);
#endif
	}
}

static ssize_t pm_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;

	i4Ret = kstrtoint(buf, 10, &g_i4PM);

	if (i4Ret)
		DBGLOG(INIT, ERROR, "sscanf pm fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set PM to %d.\n",
			g_i4PM);

		pm_EnterCtiaMode();
	}

	return (i4Ret == 0) ? count : 0;
}

static ssize_t macaddr_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(aucMacAddrOverride),
		"%s", aucMacAddrOverride);
}

static ssize_t macaddr_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;
	uint8_t aucMacAddrTemp[] = "FF:FF:FF:FF:FF:FF";

	if (count < sizeof(aucMacAddrTemp) - 1) {
		DBGLOG(INIT, ERROR, "length is too small(len=%d)\n", count);
		return -EINVAL;
	}

	kalMemCopy(&aucMacAddrTemp, buf, sizeof(aucMacAddrTemp) - 1);
	i4Ret = sscanf((uint8_t *)&aucMacAddrTemp, "%17s",
		(uint8_t *)&aucMacAddrOverride);

	if (!i4Ret)
		DBGLOG(INIT, ERROR, "sscanf mac format fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set macaddr to %s.\n",
			aucMacAddrOverride);
	}

	fgIsMacAddrOverride = TRUE;

	return (i4Ret > 0) ? count : 0;
}

static ssize_t wifiver_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(acVerInfo), "%s",
		acVerInfo);
}

static ssize_t softap_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, sizeof(acSoftAPInfo), "%s", acSoftAPInfo);
}

static ssize_t memdump_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(g_u4Memdump),
		"%d", g_u4Memdump);
}

static ssize_t memdump_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;

	i4Ret = kstrtouint(buf, 10, &g_u4Memdump);

	if (i4Ret)
		DBGLOG(INIT, ERROR, "sscanf memdump fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set memdump to %d.\n",
			g_u4Memdump);
#if IS_ENABLED(CFG_MTK_WIFI_CONNV3_SUPPORT)
		connv3_coredump_set_memdump_mode(g_u4Memdump);
#endif
	}

	return (i4Ret == 0) ? count : 0;
}

static ssize_t ant_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(g_i4Ant),
		"%d", g_i4Ant);
}


static void sysAntSetMode(void)
{
	struct ADAPTER *prAdapter = NULL;
	char input[4] = {0};
	int ret = 0;

	if (!g_prGlueInfo)
		DBGLOG(INIT, ERROR, "g_prGlueInfo is null\n");
	else if (g_i4Ant == -1)
		DBGLOG(INIT, TRACE, "keep default\n");
	else {
		prAdapter = g_prGlueInfo->prAdapter;
		ret = snprintf(input, sizeof(g_i4Ant), "%d", g_i4Ant-1);
		if (ret < 0 || ret >= sizeof(g_i4Ant)) {
			DBGLOG(INIT, ERROR, "snprintf error\n");
		}
		else
			wlanCfgSet(g_prGlueInfo->prAdapter,
				"SpeIdxCtrl", input, WLAN_CFG_DEFAULT);

		prAdapter->rWifiVar.ucNSS = (g_i4Ant == 3) ? 2 : 1;
		wlanCfgSetUint32(prAdapter, "Nss", prAdapter->rWifiVar.ucNSS);
		wlanCfgSetUint32(prAdapter, "SGCfg", 0);
	}
}

static ssize_t ant_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;

	i4Ret = kstrtoint(buf, 10, &g_i4Ant);

	if (i4Ret)
		DBGLOG(INIT, ERROR, "sscanf ant fail u4Ret=%d\n", i4Ret);
	else {
		DBGLOG(INIT, INFO,
			"Set ANT to %d.\n",
			g_i4Ant);
	}

	return (i4Ret == 0) ? count : 0;
}

#if SYS_FW_LOG_SUPPORT
static unsigned int fw_log_wifi_WtF_poll(struct file *filp, poll_table *wait)
{
	struct fw_log_wifi_WtF_interface *prWtFInf = &fw_log_wifi_WtF_inf;

	poll_wait(filp, &prWtFInf->wq, wait);
	if (prWtFInf->fgIsAllowFWLogDump) {
		DBGLOG_LIMITED(INIT, INFO, "Write FW log to file!");
		return POLLIN|POLLRDNORM;
	}
	return 0;
}

void fw_log_wifi_write_log_to_file(int32_t i4DumpInProgress)
{
	struct fw_log_wifi_WtF_interface *prWtFInf = &fw_log_wifi_WtF_inf;

	wake_up_interruptible(&prWtFInf->wq);
	prWtFInf->fgIsAllowFWLogDump = i4DumpInProgress;
}

int sysFwLogInit(void)
{
	struct fw_log_wifi_WtF_interface *prWtFInf = &fw_log_wifi_WtF_inf;
	int ret = 0;

	ret = alloc_chrdev_region(&prWtFInf->devno, 0, 1,
		FW_LOG_WIFI_WTF_INF_NAME);
	if (ret) {
		DBGLOG(INIT, ERROR,
			"alloc_chrdev_region failed, ret: %d\n",
			ret);
		goto return_fn;
	}

	cdev_init(&prWtFInf->cdev, &fw_log_wifi_WtF_fops);
	prWtFInf->cdev.owner = THIS_MODULE;

	ret = cdev_add(&prWtFInf->cdev, prWtFInf->devno, 1);
	if (ret) {
		DBGLOG(INIT, ERROR,
			"cdev_add failed, ret: %d\n",
			ret);
		goto unregister_chrdev_region;
	}

	prWtFInf->driver_class = class_create(THIS_MODULE,
		FW_LOG_WIFI_WTF_INF_NAME);
	if (IS_ERR(prWtFInf->driver_class)) {
		DBGLOG(INIT, ERROR,
			"class_create failed, ret: %d\n",
			ret);
		ret = PTR_ERR(prWtFInf->driver_class);
		goto cdev_del;
	}

	prWtFInf->class_dev = device_create(prWtFInf->driver_class,
		NULL, prWtFInf->devno, NULL, FW_LOG_WIFI_WTF_INF_NAME);
	if (IS_ERR(prWtFInf->class_dev)) {
		ret = PTR_ERR(prWtFInf->class_dev);
		DBGLOG(INIT, ERROR,
			"class_device_create failed, ret: %d\n",
			ret);
		goto class_destroy;
	}

	prWtFInf->fgIsAllowFWLogDump = 0;
	init_waitqueue_head(&prWtFInf->wq);

	goto return_fn;

class_destroy:
	class_destroy(prWtFInf->driver_class);
cdev_del:
	cdev_del(&prWtFInf->cdev);
unregister_chrdev_region:
	unregister_chrdev_region(prWtFInf->devno, 1);
return_fn:
	if (ret)
		DBGLOG(INIT, ERROR, "ret: %d\n",
			ret);

	return ret;
}

void sysFwLogUninit(void)
{
	struct fw_log_wifi_WtF_interface *prWtFInf = &fw_log_wifi_WtF_inf;

	device_destroy(prWtFInf->driver_class, prWtFInf->devno);
	class_destroy(prWtFInf->driver_class);
	cdev_del(&prWtFInf->cdev);
	kfree(&prWtFInf->cdev);
	unregister_chrdev_region(prWtFInf->devno, 1);
}

static ssize_t logDumpStatus_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(i4DumpInProgress),
		"%d", i4DumpInProgress);
}

static ssize_t logDumpStatus_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;
	int32_t i4Value = 0;

	i4Ret = kstrtouint(buf, 10, &i4Value);

	if (i4Ret)
		DBGLOG(INIT, ERROR,
			"sscanf dump status fail i4Ret=%d\n", i4Ret);
	else {
		switch (i4Value) {
		case 0:
			DBGLOG(INIT, TRACE,
				"Write fw log to file done.\n");
			i4DumpInProgress = i4Value;
			fw_log_wifi_write_log_to_file(i4DumpInProgress);
			break;
		case 1:
			DBGLOG(INIT, TRACE,
				"Write fw log to file start.\n");
			i4DumpInProgress = i4Value;
			fw_log_wifi_write_log_to_file(i4DumpInProgress);
			break;

		default:
			DBGLOG(INIT, ERROR,
				"Unknown status: %d\n",
				i4Value);
		}
	}

	return (i4Ret == 0) ? count : 0;
}
#endif

#if CFG_HDM_WIFI_SUPPORT
static ssize_t hdmwifi_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(g_hdm_wlan_loader),
		"%d", g_hdm_wlan_loader);
}

static ssize_t hdmwifi_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int32_t i4Ret = 0;
	int32_t i4Value = 0;

	i4Ret = kstrtouint(buf, 10, &i4Value);

	if (i4Ret)
		DBGLOG(INIT, ERROR,
			"sscanf dump status fail i4Ret=%d\n", i4Ret);
	else {
		switch (i4Value) {
		case 0:
			DBGLOG(INIT, INFO, "g_hdm_wlan_loader=0(Lock)\n");
			g_hdm_wlan_loader = 0;
			break;
		case 1:
			DBGLOG(INIT, INFO, "g_hdm_wlan_loader=1(Unlock)\n");
			g_hdm_wlan_loader = 1;
			break;
		default:
			DBGLOG(INIT, ERROR, "Unknown value: %d\n", i4Value);
		}
	}

	return (i4Ret == 0) ? count : 0;
}
#endif

static ssize_t feature_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf, sizeof(acFeatureInfo), "%s", acFeatureInfo);
}

#if CFG_SUPPORT_CABLE_DETECT
void cable_detect_gpio_parse(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6989-consys-atf");
	if (!node) {
		DBGLOG(INIT, ERROR, "parse wifi_cable_detect fail\n");
		return;
	}

	g_i4MainCableDetectGpio = of_get_named_gpio(node, "cable_detect_gpio_main", 0);
	if (g_i4MainCableDetectGpio < 0) {
		DBGLOG(INIT, ERROR, "parse main gpio_num fail\n");
		g_i4MainCableDetectGpio = -1;
	}

	g_i4SubCableDetectGpio = of_get_named_gpio(node, "cable_detect_gpio_sub", 0);
	if (g_i4SubCableDetectGpio < 0) {
		DBGLOG(INIT, ERROR, "parse sub gpio_num fail\n");
		g_i4SubCableDetectGpio = -1;
	}

	DBGLOG(INIT, INFO, "Main gpio_num is %d\n", g_i4MainCableDetectGpio);
	DBGLOG(INIT, INFO, "Sub  gpio_num is %d\n", g_i4SubCableDetectGpio);

	return;

}
static u_int8_t cable_detect_gpio_read_main(void)
{
	/*read GPIO Main*/
	if (gpio_is_valid(g_i4MainCableDetectGpio)) {
		if (gpio_get_value(g_i4MainCableDetectGpio))
			return CABLE_STATUS_PULL_OUT;
		else
			return CABLE_STATUS_PLUG_IN;
	}

	return CABLE_STATUS_UNKNOWN;
}

static u_int8_t cable_detect_gpio_read_sub(void)
{
	/*read GPIO Sub*/
	if (gpio_is_valid(g_i4SubCableDetectGpio)) {
		if (gpio_get_value(g_i4SubCableDetectGpio))
			return CABLE_STATUS_PULL_OUT;
		else
			return CABLE_STATUS_PLUG_IN;
	}

	return CABLE_STATUS_UNKNOWN;
}

static ssize_t wificable_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	/*read GPIO Main status*/
	aucMainCableDetectStatus = cable_detect_gpio_read_main();
	DBGLOG(INIT, INFO, "aucMainCableDetectStatus %d\n", aucMainCableDetectStatus);

	/*read GPIO Sub status*/
	aucSubCableDetectStatus = cable_detect_gpio_read_sub();
	DBGLOG(INIT, INFO, "aucSubCableDetectStatus %d\n", aucSubCableDetectStatus);

	switch (aucMainCableDetectStatus) {
	case CABLE_STATUS_PLUG_IN:
		g_CableInfo[0] = 'E';
		break;
	case CABLE_STATUS_PULL_OUT:
		g_CableInfo[0] = 'D';
		break;
	default:
		g_CableInfo[0] = 'U';
	}

	switch (aucSubCableDetectStatus) {
	case CABLE_STATUS_PLUG_IN:
		g_CableInfo[1] = 'E';
		break;
	case CABLE_STATUS_PULL_OUT:
		g_CableInfo[1] = 'D';
		break;
	default:
		g_CableInfo[1] = 'U';
	}

	return snprintf(buf, sizeof(g_CableInfo), "%s", g_CableInfo);
}
#endif

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

static struct kobj_attribute macaddr_attr
	= __ATTR(mac_addr, 0664, macaddr_show, macaddr_store);

static struct kobj_attribute wifiver_attr
	= __ATTR(wifiver, 0664, wifiver_show, NULL);

static struct kobj_attribute softap_attr
	= __ATTR(softap, 0664, softap_show, NULL);

static struct kobj_attribute pm_attr
	= __ATTR(pm, 0664, pm_show, pm_store);

static struct kobj_attribute memdump_attr
	= __ATTR(memdump, 0664, memdump_show, memdump_store);

static struct kobj_attribute ant_attr
	= __ATTR(ant, 0664, ant_show, ant_store);

#if SYS_FW_LOG_SUPPORT
static struct kobj_attribute logver_attr
	= __ATTR(dump_in_progress, 0664,
	logDumpStatus_show, logDumpStatus_store);
#endif

static struct kobj_attribute feature_attr
	= __ATTR(feature, 0664, feature_show, NULL);

#if CFG_SUPPORT_CABLE_DETECT
static struct kobj_attribute wificable_attr
	= __ATTR(wificable, 0664, wificable_show, NULL);
#endif

#if CFG_HDM_WIFI_SUPPORT
static struct kobj_attribute hdmwifi_attr
	= __ATTR(hdm_wlan_loader, 0664,
	hdmwifi_show, hdmwifi_store);
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if CFG_SUPPORT_CABLE_DETECT
void sysInitCableInfo(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &wificable_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create cableinfo entry\n");
}

void sysUninitCableInfo(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &wificable_attr.attr);
}

#endif

void sysCreateMacAddr(void)
{
	if (g_prGlueInfo) {
		uint8_t rMacAddr[MAC_ADDR_LEN];

		COPY_MAC_ADDR(rMacAddr,
			g_prGlueInfo->prAdapter->rWifiVar.aucMacAddress);

		kalSnprintf(aucMacAddrOverride,
			sizeof(aucMacAddrOverride),
			"%pM",
			rMacAddr);

		DBGLOG(INIT, TRACE,
			"Init macaddr to " MACSTR ".\n",
			MAC2STR(rMacAddr));
	}
}

void sysInitMacAddr(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &macaddr_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create macaddr entry\n");
}

void sysUninitMacAddr(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &macaddr_attr.attr);
}

void sysInitPM(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &pm_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create macaddr entry\n");
}

void sysUninitPM(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &pm_attr.attr);
}

void sysCreateWifiVer(void)
{
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

	char aucDriverVersionStr[] = STR(NIC_DRIVER_MAJOR_VERSION) "_"
		STR(NIC_DRIVER_MINOR_VERSION) "_"
		STR(NIC_DRIVER_SERIAL_VERSION);
	uint16_t u2NvramVer = 0;
	uint8_t ucOffset = 0;

	kalMemZero(acVerInfo, sizeof(acVerInfo));

	ucOffset += kalSnprintf(acVerInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "Mediatek DRIVER_VER: %s\n", aucDriverVersionStr);

	if (g_prGlueInfo)
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "FW_VER: %s\n"
			, g_prGlueInfo->prAdapter->rVerInfo.aucReleaseManifest);
	else {
		if (g_wifiVer_length == 0)
			ucOffset += kalSnprintf(acVerInfo + ucOffset,
				MTK_INFO_MAX_SIZE - ucOffset,
				"FW_VER: %s\n", aucDefaultFWVersion);
		else
			ucOffset += kalSnprintf(acVerInfo + ucOffset,
				MTK_INFO_MAX_SIZE - ucOffset,
				"FW_VER: %s\n", g_wifiVer);
	}

	if (g_prGlueInfo) {
		kalCfgDataRead16(g_prGlueInfo,
			OFFSET_OF(struct WIFI_CFG_PARAM_STRUCT,
			u2Part1OwnVersion), &u2NvramVer);
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "NVRAM: 0x%x\n", u2NvramVer);
	} else {
		ucOffset += kalSnprintf(acVerInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "NVRAM: Unknown\n");
	}
}

void sysInitWifiVer(void)
{
	struct mt66xx_chip_info *prChipInfo;
	int32_t i4Ret = 0;

	/* Get wifi version */
	glGetChipInfo((void **)&prChipInfo);
	if (prChipInfo && prChipInfo->fw_dl_ops->getFwVerInfo)
		prChipInfo->fw_dl_ops->getFwVerInfo(g_wifiVer,
			&g_wifiVer_length, MANIFEST_BUFFER_SIZE);

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &wifiver_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create wifiver entry\n");

	sysCreateWifiVer();
}

void sysUninitWifiVer(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &wifiver_attr.attr);
}

void sysCreateSoftap(void)
{
	struct REG_INFO *prRegInfo = NULL;

	uint8_t ucOffset = 0;
	u_int8_t fgDbDcModeEn = FALSE;

	/* Log SoftAP/hotspot information into .softap.info
	 * #Support wifi and hotspot at the same time?
	 * DualBandConcurrency=no
	 * # Supporting 5Ghz
	 * 5G=check NVRAM ucEnable5GBand
	 * # Max support client count
	 * maxClient=P2P_MAXIMUM_CLIENT_COUNT
	 * #Supporting android_net_wifi_set_Country_Code_Hal
	 * HalFn_setCountryCodeHal=yes ,
	 * call mtk_cfg80211_vendor_set_country_code
	 * #Supporting android_net_wifi_getValidChannels
	 * HalFn_getValidChannels=yes,
	 * call mtk_cfg80211_vendor_get_channel_list
	*/

	if (g_prGlueInfo) {
		prRegInfo = &(g_prGlueInfo->rRegInfo);
#if CFG_SUPPORT_DBDC
#if (CFG_SUPPORT_WIFI_6G == 1) && (CFG_SUPPORT_802_11AX == 1)
		if (g_prGlueInfo->prAdapter->rWifiVar.eDbdcMode != ENUM_DBDC_MODE_DISABLED) {
			fgDbDcModeEn = TRUE;
		}
#else
		fgDbDcModeEn = g_prGlueInfo->prAdapter->rWifiVar.fgDbDcModeEn;
#endif
#endif
	}

	kalMemZero(acSoftAPInfo, sizeof(acSoftAPInfo));

	ucOffset = 0;

	if (g_prGlueInfo) {
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "DualBandConcurrency=%s\n"
			, fgDbDcModeEn ? "yes" : "no");
	} else
#if (CFG_SUPPORT_WIFI_6G == 1) && (CFG_SUPPORT_802_11AX == 1)
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "DualBandConcurrency=yes\n");
#else
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "DualBandConcurrency=no\n");
#endif

	if (prRegInfo)
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "5G=%s\n", prRegInfo->ucEnable5GBand ? "yes" : "no");
	else
		ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "5G=yes\n");

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "maxClient=%d\n", P2P_MAXIMUM_CLIENT_COUNT);

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "HalFn_setCountryCodeHal=%s\n", "yes");

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "HalFn_getValidChannels=%s\n", "yes");

	ucOffset += kalSnprintf(acSoftAPInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "DualInterface=%s\n", "yes");
}

void sysInitSoftap(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &softap_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create softap entry\n");

	sysCreateSoftap();
}

void sysUninitSoftap(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &softap_attr.attr);
}

void sysInitMemdump(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &memdump_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create softap entry\n");
}

void sysUninitMemdump(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &memdump_attr.attr);
}

void sysInitAnt(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &ant_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR, "Unable to create macaddr entry\n");
}

void sysUninitAnt(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &ant_attr.attr);
}

#if SYS_FW_LOG_SUPPORT
void sysInitDumpInProgress(void)
{
	int32_t i4Ret = 0;

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	i4Ret = sysfs_create_file(wifi_kobj, &logver_attr.attr);
	if (i4Ret)
		DBGLOG(INIT, ERROR,
			"Unable to create dump_in_progress entry\n");
}

void sysUninitDumpInProgress(void)
{
	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &logver_attr.attr);
}
#endif

#if CFG_HDM_WIFI_SUPPORT
static u_int8_t g_fgHdmSysfsCreated = FALSE;

void HdmWifi_SysfsInit(void)
{
	int32_t i4Ret = 0;

	if (g_fgHdmSysfsCreated) {
		DBGLOG(INIT, VOC, "hdm_wlan_loader existed already\n");
		return;
	}

	i4Ret = sysfs_create_file(kernel_kobj, &hdmwifi_attr.attr);
	if (i4Ret) {
		DBGLOG(INIT, VOC,
			"Unable to create hdm_wlan_loader\n");
	}
	else {
		DBGLOG(INIT, VOC, "Created hdm_wlan_loader\n");
		g_fgHdmSysfsCreated = TRUE;
	}
}

void HdmWifi_SysfsUninit(void)
{
	if (g_fgHdmSysfsCreated) {
		sysfs_remove_file(kernel_kobj, &hdmwifi_attr.attr);
		g_fgHdmSysfsCreated = FALSE;
		DBGLOG(INIT, VOC, "Removed hdm_wlan_loader\n");
	}
	else
		DBGLOG(INIT, VOC, "No hdm_wlan_loader\n");
}
#endif

struct FS_SW_ILD_T g_SWFeatureTblWifi7[] = {
	{FS_SW_PNO_ID, FS_SW_PNO_LEN,
		{FS_SW_PNO_SUPPORT|FS_SW_PNO_UNASSOC|FS_SW_PNO_ASSOC} },
	{FS_SW_TWT_ID, FS_SW_TWT_LEN,
		{0, 0} },
	{FS_SW_OPTI_ID, FS_SW_OPTI_LEN,
		{FS_SW_OPTI_SUPPORT} },
	{FS_SW_SCHE_PM_ID, FS_SW_SCHE_PM_LEN,
		{0} },
	{FS_SW_D_WAKEUP_ID, FS_SW_D_WAKEUP_LEN,
		{FS_SW_D_WAKEUP_SUPPORT} },
	{FS_SW_RFC8325_ID, FS_SW_RFC8325_LEN,
		{FS_SW_RFC8325_SUPPORT} },
	{FS_SW_MHS_ID, FS_SW_MHS_LEN,
		{FS_SW_MHS_GET_VALID_CH|FS_SW_MHS_WPA3|FS_SW_MHS_11AX,
		FS_SW_MHS_DUAL_INF|FS_SW_MHS_5G|FS_SW_MHS_6G|
		(FS_SW_MHS_MAX_CLIENT<<FS_SW_MHS_MAX_CLIENT_OFFSET)|
		FS_SW_MHS_CC_HAL} },
	{FS_SW_ROAM_ID, FS_SW_ROAM_LEN,
		{FS_SW_ROAM_VER_MAJOR, FS_SW_ROAM_VER_MINOR,
		FS_SW_ROAM_3B_B0|FS_SW_ROAM_3B_B1|FS_SW_ROAM_3B_B2|
		FS_SW_ROAM_3B_B3|FS_SW_ROAM_3B_B4|FS_SW_ROAM_3B_B5|
		FS_SW_ROAM_3B_B6|FS_SW_ROAM_3B_B7,
		FS_SW_ROAM_4B_B0|FS_SW_ROAM_4B_B1|FS_SW_ROAM_4B_B2|
		FS_SW_ROAM_4B_B3} },
	{FS_SW_NCHO_ID, FS_SW_NCHO_LEN,
		{FS_SW_NCHO_VER_MAJOR, FS_SW_NCHO_VER_MINOR} },
	{FS_SW_ASSUR_ID, FS_SW_ASSUR_LEN,
		{FS_SW_ASSUR_SUPPORT} },
	{FS_SW_FRAMEPCAP_ID, FS_SW_FRAMEPCAP_LEN,
		{0} },
	{FS_SW_SEC_ID, FS_SW_SEC_LEN,
		{FS_SW_SEC_WPA3_SAE_HASH|FS_SW_SEC_WPA3_SAE_FT|
		FS_SW_SEC_WPA3_ENT_S_B_192|FS_SW_SEC_FILS_SHA256|
		FS_SW_SEC_FILS_SHA384,FS_SW_SEC_ENHANCED_OPEN} },
	{FS_SW_P2P_ID, FS_SW_P2P_LEN,
		{FS_SW_P2P_NAN6E_STD|FS_SW_P2P_NAN6E_SS|
		FS_SW_P2P_NAN_REL_VER << FS_SW_P2P_NAN_REL_VER_OFFSET,
		0, FS_SW_P2P_STA_P2P_TDLS|FS_SW_P2P_STA_SAP_TDLS|
		FS_SW_P2P_STA_SAP_P2P_TDLS|FS_SW_P2P_STA_P2P_P2P_TDLS,
		FS_SW_P2P_STA_P2P|FS_SW_P2P_STA_SAP|FS_SW_P2P_STA_NAN|
		FS_SW_P2P_STA_TDLS|FS_SW_P2P_STA_SAP_P2P|FS_SW_P2P_STA_P2P_P2P,
		FS_SW_P2P_TDLS_MAX_NUM|
		FS_SW_P2P_NAN_NDP_MAX_WIFI7<<FS_SW_P2P_NAN_NDP_MAX_OFFSET,
		FS_SW_P2P_NAN|FS_SW_P2P_TDLS|FS_SW_P2P_P2P6E|
		FS_SW_P2P_LISTEN_OFFLOAD|FS_SW_P2P_LISTEN_OFFLOAD|
		FS_SW_P2P_NOA_PS|FS_SW_P2P_TDLS_OFF_CH|
		FS_SW_P2P_TDLS_CAP_ENHANCE|FS_SW_P2P_TDLS_PS} },
	{FS_SW_BIGD_ID, FS_SW_BIGD_LEN,
		{FS_SW_BIGD_GETBSSINFO|FS_SW_BIGD_GETASSOCREJECTINFO|
		FS_SW_BIGD_GETSTAINFO} },
	{FS_SW_GETSTADUMP_ID, FS_SW_GETSTADUMP_LEN,
		{FS_SW_GETSTADUMP_SUPPORT} },
	{FS_SW_STDPLUS_ID, FS_SW_STDPLUS_LEN,
		{FS_SW_STDPLUS_VER_MAJOR, FS_SW_STDPLUS_VER_MINOR,
		0, 0, 0 /*FS_SW_STDPLUS_ESDF*/,
		/*FS_SW_STDPLUS_FR|FS_SW_STDPLUS_FC|*/FS_SW_STDPLUS_EHT2,
		FS_SW_STDPLUS_ASC|/*FS_SW_STDPLUS_AMC|*/
		FS_SW_STDPLUS_ASCC|FS_SW_STDPLUS_ADSDC|
		/*FS_SW_STDPLUS_PAIRING|*/FS_SW_STDPLUS_APNAN} },
};

struct FS_SW_ILD_T g_SWFeatureTblWifi6e[] = {
	{FS_SW_PNO_ID, FS_SW_PNO_LEN,
		{FS_SW_PNO_SUPPORT|FS_SW_PNO_UNASSOC|FS_SW_PNO_ASSOC} },
	{FS_SW_TWT_ID, FS_SW_TWT_LEN,
		{0, 0} },
	{FS_SW_OPTI_ID, FS_SW_OPTI_LEN,
		{FS_SW_OPTI_SUPPORT} },
	{FS_SW_SCHE_PM_ID, FS_SW_SCHE_PM_LEN,
		{0} },
	{FS_SW_D_WAKEUP_ID, FS_SW_D_WAKEUP_LEN,
		{FS_SW_D_WAKEUP_SUPPORT} },
	{FS_SW_RFC8325_ID, FS_SW_RFC8325_LEN,
		{FS_SW_RFC8325_SUPPORT} },
	{FS_SW_MHS_ID, FS_SW_MHS_LEN,
		{FS_SW_MHS_GET_VALID_CH|FS_SW_MHS_WPA3|FS_SW_MHS_11AX,
		FS_SW_MHS_DUAL_INF|FS_SW_MHS_5G|FS_SW_MHS_6G|
		(FS_SW_MHS_MAX_CLIENT<<FS_SW_MHS_MAX_CLIENT_OFFSET)|
		FS_SW_MHS_CC_HAL} },
	{FS_SW_ROAM_ID, FS_SW_ROAM_LEN,
		{FS_SW_ROAM_VER_MAJOR, FS_SW_ROAM_VER_MINOR,
		FS_SW_ROAM_3B_B0|FS_SW_ROAM_3B_B1|FS_SW_ROAM_3B_B2|
		FS_SW_ROAM_3B_B3|FS_SW_ROAM_3B_B4|FS_SW_ROAM_3B_B5|
		FS_SW_ROAM_3B_B6|FS_SW_ROAM_3B_B7,
		FS_SW_ROAM_4B_B0|FS_SW_ROAM_4B_B1|FS_SW_ROAM_4B_B2|
		FS_SW_ROAM_4B_B3} },
	{FS_SW_NCHO_ID, FS_SW_NCHO_LEN,
		{FS_SW_NCHO_VER_MAJOR, FS_SW_NCHO_VER_MINOR} },
	{FS_SW_ASSUR_ID, FS_SW_ASSUR_LEN,
		{FS_SW_ASSUR_SUPPORT} },
	{FS_SW_FRAMEPCAP_ID, FS_SW_FRAMEPCAP_LEN,
		{0} },
	{FS_SW_SEC_ID, FS_SW_SEC_LEN,
		{FS_SW_SEC_WPA3_SAE_HASH|FS_SW_SEC_WPA3_SAE_FT|
		FS_SW_SEC_WPA3_ENT_S_B_192|FS_SW_SEC_FILS_SHA256|
		FS_SW_SEC_FILS_SHA384,FS_SW_SEC_ENHANCED_OPEN} },
	{FS_SW_P2P_ID, FS_SW_P2P_LEN,
		{FS_SW_P2P_NAN6E_STD|FS_SW_P2P_NAN6E_SS|
		FS_SW_P2P_NAN_REL_VER << FS_SW_P2P_NAN_REL_VER_OFFSET,
		0, FS_SW_P2P_STA_P2P_TDLS|FS_SW_P2P_STA_SAP_TDLS|
		FS_SW_P2P_STA_SAP_P2P_TDLS|FS_SW_P2P_STA_P2P_P2P_TDLS,
		FS_SW_P2P_STA_P2P|FS_SW_P2P_STA_SAP|FS_SW_P2P_STA_NAN|
		FS_SW_P2P_STA_TDLS|FS_SW_P2P_STA_SAP_P2P|FS_SW_P2P_STA_P2P_P2P,
		FS_SW_P2P_TDLS_MAX_NUM|
		FS_SW_P2P_NAN_NDP_MAX_WIFI6E<<FS_SW_P2P_NAN_NDP_MAX_OFFSET,
		FS_SW_P2P_NAN|FS_SW_P2P_TDLS|FS_SW_P2P_P2P6E|
		FS_SW_P2P_LISTEN_OFFLOAD|FS_SW_P2P_LISTEN_OFFLOAD|
		FS_SW_P2P_NOA_PS|FS_SW_P2P_TDLS_OFF_CH|
		FS_SW_P2P_TDLS_CAP_ENHANCE|FS_SW_P2P_TDLS_PS} },
	{FS_SW_BIGD_ID, FS_SW_BIGD_LEN,
		{FS_SW_BIGD_GETBSSINFO|FS_SW_BIGD_GETASSOCREJECTINFO|
		FS_SW_BIGD_GETSTAINFO} },
	{FS_SW_GETSTADUMP_ID, FS_SW_GETSTADUMP_LEN,
		{FS_SW_GETSTADUMP_SUPPORT} },
	{FS_SW_STDPLUS_ID, FS_SW_STDPLUS_LEN,
		{FS_SW_STDPLUS_VER_MAJOR, FS_SW_STDPLUS_VER_MINOR,
		0, 0, 0 /*FS_SW_STDPLUS_ESDF*/,
		0/*FS_SW_STDPLUS_FR|FS_SW_STDPLUS_FC|FS_SW_STDPLUS_EHT2*/,
		FS_SW_STDPLUS_ASC|/*FS_SW_STDPLUS_AMC|*/
		FS_SW_STDPLUS_ASCC|FS_SW_STDPLUS_ADSDC|
		/*FS_SW_STDPLUS_PAIRING|*/FS_SW_STDPLUS_APNAN} },
};

struct FS_SW_ILD_T g_SWFeatureTbl[] = {
	{FS_SW_PNO_ID, FS_SW_PNO_LEN,
		{FS_SW_PNO_SUPPORT|FS_SW_PNO_UNASSOC|FS_SW_PNO_ASSOC} },
	{FS_SW_TWT_ID, FS_SW_TWT_LEN,
		{0} },
	{FS_SW_OPTI_ID, FS_SW_OPTI_LEN,
		{0} },
	{FS_SW_SCHE_PM_ID, FS_SW_SCHE_PM_LEN,
		{0} },
	{FS_SW_D_WAKEUP_ID, FS_SW_D_WAKEUP_LEN,
		{0} },
	{FS_SW_RFC8325_ID, FS_SW_RFC8325_LEN,
		{FS_SW_RFC8325_SUPPORT} },
	{FS_SW_MHS_ID, FS_SW_MHS_LEN,
		{FS_SW_MHS_GET_VALID_CH|FS_SW_MHS_WPA3,
		FS_SW_MHS_DUAL_INF|FS_SW_MHS_5G|
		(FS_SW_MHS_MAX_CLIENT<<FS_SW_MHS_MAX_CLIENT_OFFSET)|
		FS_SW_MHS_CC_HAL} },
	{FS_SW_ROAM_ID, FS_SW_ROAM_LEN,
		{FS_SW_ROAM_VER_MAJOR, FS_SW_ROAM_VER_MINOR,
		FS_SW_ROAM_3B_B0|FS_SW_ROAM_3B_B1|FS_SW_ROAM_3B_B2|
		FS_SW_ROAM_3B_B3|FS_SW_ROAM_3B_B6|FS_SW_ROAM_3B_B7,
		FS_SW_ROAM_4B_B0|FS_SW_ROAM_4B_B1|FS_SW_ROAM_4B_B2|
		FS_SW_ROAM_4B_B3} },
	{FS_SW_NCHO_ID, FS_SW_NCHO_LEN,
		{FS_SW_NCHO_VER_MAJOR, FS_SW_NCHO_VER_MINOR} },
	{FS_SW_ASSUR_ID, FS_SW_ASSUR_LEN,
		{FS_SW_ASSUR_SUPPORT} },
	{FS_SW_FRAMEPCAP_ID, FS_SW_FRAMEPCAP_LEN,
		{0} },
	{FS_SW_SEC_ID, FS_SW_SEC_LEN,
		{FS_SW_SEC_WPA3_SAE_HASH, FS_SW_SEC_ENHANCED_OPEN} },
	{FS_SW_P2P_ID, FS_SW_P2P_LEN,
		{0, 0, 0,
		FS_SW_P2P_STA_P2P|FS_SW_P2P_STA_SAP|FS_SW_P2P_STA_TDLS,
		FS_SW_P2P_TDLS_MAX_NUM,
		FS_SW_P2P_TDLS|FS_SW_P2P_TDLS_PS} },
	{FS_SW_BIGD_ID, FS_SW_BIGD_LEN,
		{FS_SW_BIGD_GETBSSINFO|FS_SW_BIGD_GETSTAINFO} },
	{FS_SW_GETSTADUMP_ID, FS_SW_GETSTADUMP_LEN,
		{0} },
	{FS_SW_STDPLUS_ID, FS_SW_STDPLUS_LEN,
		{FS_SW_STDPLUS_VER_MAJOR, FS_SW_STDPLUS_VER_MINOR,
		0, 0, 0 /*FS_SW_STDPLUS_ESDF*/,
		0/*FS_SW_STDPLUS_FR|FS_SW_STDPLUS_FC|FS_SW_STDPLUS_EHT2*/,
		FS_SW_STDPLUS_ASC|/*FS_SW_STDPLUS_AMC|*/
		FS_SW_STDPLUS_ASCC|FS_SW_STDPLUS_ADSDC|
		/*FS_SW_STDPLUS_PAIRING|*/FS_SW_STDPLUS_APNAN} },
};

u_int8_t sysIsStdplusEnable(void)
{
	uint8_t *pucConfigBuf = NULL;
	uint32_t u4ConfigReadLen = 0;
	void *pvDev = NULL;

	kalGetPlatDev(&pvDev);
	if (pvDev == NULL) {
		DBGLOG(INIT, WARN, "glGetPlatDev failed\n");
		//return FALSE;
	}
	if (kalRequestFirmware("stdplus_disable.cfg",
		&pucConfigBuf,
	    &u4ConfigReadLen,
	    TRUE,
	    pvDev) == 0) {
		DBGLOG(INIT, INFO, "[Feature] Disable STD+\n");
		if (pucConfigBuf)
			kalMemFree(pucConfigBuf, VIR_MEM_TYPE, u4ConfigReadLen);
		return FALSE;
	}

#if CFG_SUPPORT_NAN_EXT
	return TRUE;
#else
	return FALSE;
#endif
}

void sysCreateFeature(void)
{
	int			i, j;
	u_int16_t	fs_hw_feature = 0;
	u_int8_t	ucOffset = 0;
	struct FS_SW_ILD_T *prSwFsTbl;

	DBGLOG(INIT, INFO, "[%s]\n", __func__);

	kalMemZero(acFeatureInfo, sizeof(acFeatureInfo));

	ucOffset = 0;

#if defined(CFG_FS_WIFI6E_MIMO)
	DBGLOG(INIT, INFO, "[Feature] CFG_FS_WIFI6E_MIMO\n");
	/* WIFI6E & MIMO */
	fs_hw_feature =
	(glIsWiFi7CfgFile() ? FS_HW_STANDARD_WIFI7 : FS_HW_STANDARD_WIFI6E)
	| FS_HW_NUM_CORES_TWO << FS_HW_NUM_CORES_OFFSET
	| FS_HW_CONCURRENCY_MODE_ONE << FS_HW_CONCURRENCY_MODE_OFFSET
	| FS_HW_NUM_ANT_MIMO << FS_HW_NUM_ANT_OFFSET;
#elif defined(CFG_FS_WIFI6_MIMO)
	DBGLOG(INIT, INFO, "[Feature] CFG_FS_WIFI6_MIMO\n");
	/* WIFI6 & MIMO */
	fs_hw_feature =   FS_HW_STANDARD_WIFI6
	| FS_HW_NUM_CORES_ONE << FS_HW_NUM_CORES_OFFSET
	| FS_HW_CONCURRENCY_MODE_ONE << FS_HW_CONCURRENCY_MODE_OFFSET
	| FS_HW_NUM_ANT_MIMO << FS_HW_NUM_ANT_OFFSET;
#elif defined(CFG_FS_WIFI5_MIMO)
	DBGLOG(INIT, INFO, "[Feature] CFG_FS_WIFI5_MIMO\n");
	/* WIFI5 & MIMO */
	fs_hw_feature = FS_HW_STANDARD_WIFI5
	| FS_HW_NUM_CORES_ONE << FS_HW_NUM_CORES_OFFSET
	| FS_HW_NUM_ANT_MIMO << FS_HW_NUM_ANT_OFFSET;
#else
	DBGLOG(INIT, INFO, "[Feature] CFG_FS_WIFI5_SISO\n");
	/* WIFI5 & SISO */
	fs_hw_feature =   FS_HW_STANDARD_WIFI5
	| FS_HW_NUM_CORES_ONE << FS_HW_NUM_CORES_OFFSET
	| FS_HW_NUM_ANT_SISO << FS_HW_NUM_ANT_OFFSET;
#endif

	/* Feature Version */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%04X", FS_VERSION);

	/* Solution Provider */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%s", aucSolutionProvider);

	/* HW Feature Length */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%02X", FS_HW_FEATURE_LEN);

	/* HW Feature Set */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%04X", fs_hw_feature);

	/* SW Feature Length */
	ucOffset += kalSnprintf(acFeatureInfo + ucOffset
		, MTK_INFO_MAX_SIZE - ucOffset
		, "%04X", FS_SW_FEATURE_LEN);

	/* SW Feature Set */
#if defined(CFG_FS_WIFI6E_MIMO)
	if (glIsWiFi7CfgFile()) prSwFsTbl = g_SWFeatureTblWifi7;
	else prSwFsTbl = g_SWFeatureTblWifi6e;
#else
	prSwFsTbl = g_SWFeatureTbl;
#endif
	for (i = 0; i < FS_SW_FEATURE_NUM; i++) {
		ucOffset += kalSnprintf(acFeatureInfo + ucOffset
			, MTK_INFO_MAX_SIZE - ucOffset
			, "%02X%02X", prSwFsTbl[i].u8ID,
						prSwFsTbl[i].u8Len);

		for (j = 0; j < prSwFsTbl[i].u8Len; j++) {
			u_int8_t ucData =
				prSwFsTbl[i].aucData[j];

			if (FS_SW_STDPLUS_ID ==
				prSwFsTbl[i].u8ID) {
				if (!sysIsStdplusEnable())
					ucData = 0;
			}

			ucOffset += kalSnprintf(acFeatureInfo + ucOffset
				, MTK_INFO_MAX_SIZE - ucOffset
				, "%02X", ucData);
		}
	}

	DBGLOG(INIT, INFO, "[%s] Feature Set\n", acFeatureInfo);

}

void sysInitFeature(void)
{
	int32_t fsRet = 0;
	void *pvDev = NULL;
	uint8_t *pucWifi7CfgBuf = NULL;
	uint32_t u4Wifi7CfgReadLen = 0;

	DBGLOG(INIT, INFO, "[%s]\n", __func__);

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	fsRet = sysfs_create_file(wifi_kobj, &feature_attr.attr);
	if (fsRet)
		DBGLOG(INIT, ERROR,
			"Unable to create feature_attr entry\n");

	kalGetPlatDev(&pvDev);
	if (pvDev == NULL) {
		DBGLOG(INIT, ERROR, "kalGetPlatDev failed\n");
	}

	if (kalRequestFirmware("wifi7.cfg", &pucWifi7CfgBuf,
	    &u4Wifi7CfgReadLen, TRUE, pvDev) == 0) {
		DBGLOG(INIT, INFO, "wifi7.cfg file exists");
		g_IsWifi7CfgFile = TRUE;
		kalMemFree(pucWifi7CfgBuf, VIR_MEM_TYPE,
			u4Wifi7CfgReadLen);
	}
	else {
		DBGLOG(INIT, INFO, "wifi7.cfg file doesn't exists");
		g_IsWifi7CfgFile = FALSE;
	}

	sysCreateFeature();
}

void sysUninitFeature(void)
{

	DBGLOG(INIT, INFO, "[%s]\n", __func__);

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &feature_attr.attr);
}

u_int8_t glIsWiFi7CfgFile(void)
{
	return g_IsWifi7CfgFile;
}

#if CFG_SUPPORT_NAN_EXT
#define NAN_INFO_BUF_SIZE (1024)

static char acNanInfo[NAN_INFO_BUF_SIZE] = {0};

static ssize_t nan_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return snprintf(buf,
		sizeof(acNanInfo),
		"%s", acNanInfo);
}

static ssize_t nan_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
#if 0
	uint8_t acNanBytes[NAN_INFO_BUF_SIZE] = {0};

	kalMemZero(acNanInfo, sizeof(acNanInfo));

	kalMemCopy(acNanInfo, buf, count);

	DBGLOG(INIT, INFO, "Hex: %s\n", acNanInfo);

	wlanHexStrToByteArray(acNanInfo, acNanBytes, count);

	if (g_prGlueInfo)
		nanExtParseCmd(g_prGlueInfo->prAdapter,
			    acNanBytes, sizeof(acNanBytes));
#else
	/* TODO */
	struct NanExtCmdMsg extCmd = {0};

	extCmd.fwHeader.msgVersion = 1;
	//extCmd.fwHeader.msgId = 0;
	//extCmd.fwHeader.handle = 0;
	extCmd.fwHeader.transactionId = 1;
	/* Convert echo hex string to bytes as unified internal format */
	extCmd.fwHeader.msgLen =
		wlanHexStrToByteArray(buf, extCmd.data, sizeof(extCmd.data));

	if (!g_prGlueInfo)
		return 0;


	mtk_cfg80211_vendor_nan_ext(
		g_prGlueInfo->prDevHandler->ieee80211_ptr->wiphy,
		(wlanGetNetDev(g_prGlueInfo, NAN_DEFAULT_INDEX))->ieee80211_ptr,
		&extCmd, sizeof(struct NanExtCmdMsg));
#endif

	return count;
}

static struct kobj_attribute nan_attr
	= __ATTR(nan, 0664, nan_show, nan_store);

void sysInitNan(void)
{
	int32_t fsRet = 0;

	DBGLOG(INIT, INFO, "Enter %s\n", __func__);

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	fsRet = sysfs_create_file(wifi_kobj, &nan_attr.attr);
	if (fsRet)
		DBGLOG(INIT, ERROR,
			"Unable to create nan_attr entry\n");
}

void sysUninitNan(void)
{

	DBGLOG(INIT, INFO, "Enter %s\n", __func__);

	if (!wifi_kobj) {
		DBGLOG(INIT, ERROR, "wifi_kobj is null\n");
		return;
	}

	sysfs_remove_file(wifi_kobj, &nan_attr.attr);
}
#endif

int32_t sysCreateFsEntry(struct GLUE_INFO *prGlueInfo)
{
	DBGLOG(INIT, INFO, "[%s]\n", __func__);

	g_prGlueInfo = prGlueInfo;

	sysCreateMacAddr();
	pm_EnterCtiaMode();
	sysCreateWifiVer();
	sysCreateSoftap();
	sysAntSetMode();

	return 0;
}

int32_t sysRemoveSysfs(void)
{
	g_prGlueInfo = NULL;

	return 0;
}

int32_t sysInitFs(void)
{
	DBGLOG(INIT, TRACE, "[%s]\n", __func__);

	wifi_kobj = kobject_create_and_add("wifi", NULL);
	kobject_get(wifi_kobj);
	kobject_uevent(wifi_kobj, KOBJ_ADD);

	sysInitMacAddr();
	sysInitSoftap();
	sysInitPM();
	sysInitMemdump();
	sysInitAnt();
#if SYS_FW_LOG_SUPPORT
	sysInitDumpInProgress();
#endif
	sysInitFeature();
#if CFG_SUPPORT_NAN_EXT
	sysInitNan();
#endif
#if CFG_SUPPORT_CABLE_DETECT
	sysInitCableInfo();
#endif

	return 0;
}

int32_t sysUninitSysFs(void)
{
	DBGLOG(INIT, TRACE, "[%s]\n", __func__);

	sysUninitMemdump();
	sysUninitPM();
	sysUninitSoftap();
	sysUninitWifiVer();
	sysUninitMacAddr();
	sysUninitAnt();
#if SYS_FW_LOG_SUPPORT
	sysUninitDumpInProgress();
#endif
	sysUninitFeature();
#if CFG_SUPPORT_NAN_EXT
	sysUninitNan();
#endif
#if CFG_SUPPORT_CABLE_DETECT
	sysUninitCableInfo();
#endif

	kobject_put(wifi_kobj);
	kobject_uevent(wifi_kobj, KOBJ_REMOVE);
	wifi_kobj = NULL;

	return 0;
}

static void glWlanGetTraces(uint8_t *pucTraces, uint32_t u4MaxLen)
{
#define STACK_TRACE_SIZE 32
#if CONFIG_STACKTRACE
#if KERNEL_VERSION(5, 10, 0) > CFG80211_VERSION_CODE
	struct stack_trace trace;
#endif
	unsigned long au4Stacks[STACK_TRACE_SIZE];
	uint32_t u4EntryNum;
	uint32_t u4Offset = 0;
	uint32_t i;
#endif

	kalMemZero(pucTraces, u4MaxLen);

#if CONFIG_STACKTRACE
#if KERNEL_VERSION(5, 10, 0) > CFG80211_VERSION_CODE
	trace.nr_entries = 0;
	trace.entries = &au4Stacks[0];
	trace.max_entries = STACK_TRACE_SIZE;
	trace.skip = 0;

	save_stack_trace(&trace);
	u4EntryNum = trace.nr_entries;
#else
	u4EntryNum = stack_trace_save(au4Stacks, STACK_TRACE_SIZE, 0);
#endif
	for (i = 0; i < u4EntryNum; i++) {
		u4Offset += kalScnprintf(pucTraces + u4Offset,
			u4MaxLen - u4Offset,
			"%p\n", au4Stacks[i]);
	}
#else
	DBGLOG(INIT, INFO, "Kernel stack trace not support\n");
#endif
}

static void
glGetRstInfo(uint32_t *pu4Reason, uint8_t *pcData,
			uint32_t u4DataLen)
{
	uint32_t u4Offset;
	uint32_t u4RstReason = glGetRstReason();

	if (u4RstReason <= 0 || u4RstReason >= RST_REASON_MAX)
		u4RstReason = 0;

	DBGLOG(INIT, INFO, "eResetReason=%u (%s), len %lu\n",
		u4RstReason,
		apucRstReason[u4RstReason],
		kalStrLen(apucRstReason[u4RstReason]));

	*pu4Reason = u4RstReason;
	kalMemZero(pcData, u4DataLen);
	if (g_pucTraces == NULL) {
		if (u4RstReason != 0)
			kalScnprintf(pcData, u4DataLen,
				apucRstReason[u4RstReason]);
		else
			kalScnprintf(pcData, u4DataLen,
				"Unknown reason. Need to check the log.\n");
	} else {
		u4Offset = kalScnprintf(pcData, u4DataLen,
				"%s\n", apucRstReason[u4RstReason]);
		kalScnprintf(pcData + u4Offset, u4DataLen - u4Offset,
			"%s", g_pucTraces);
		kalMemFree(g_pucTraces, VIR_MEM_TYPE,
			RST_REPORT_DATA_MAX_LEN);
		g_pucTraces = NULL;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Send reset event to upper layer.
 *
 * @param none
 *
 * @retval none
 */
/*----------------------------------------------------------------------------*/
static void
glNotifyChipReset(uint32_t u4Reason, uint8_t *pcData,
			uint32_t u4DataLen)
{

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

	struct GLUE_INFO *prGlueInfo = NULL;
	struct net_device *prDev = NULL;
	struct wiphy *wiphy = NULL;
	struct wireless_dev *wdev = NULL;
	struct PARAM_HANG_INFO *list;
	uint32_t size = 0;
	uint32_t u4Offset = 0;
	uint32_t u4FwVerLen = 0;
	uint8_t ucBssIndex = 0;
	uint8_t *pucFwVer = NULL;

	char aucDriverVersionStr[] = "MTK_" STR(NIC_DRIVER_MAJOR_VERSION) "_"
					     STR(NIC_DRIVER_MINOR_VERSION) "_"
					     STR(NIC_DRIVER_SERIAL_VERSION) "-";

	char cid[] = "MT6631";

	DBGLOG(INIT, INFO, "glNotifyChipReset start\n");

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	if (prGlueInfo == NULL || !prGlueInfo->u4ReadyFlag ||
		!prGlueInfo->prAdapter) {
		DBGLOG(INIT, ERROR, "driver is not ready\n");
		return;
	}

	wiphy = wlanGetWiphy();
	if (wiphy == NULL) {
		DBGLOG(INIT, ERROR, "wiphy null\n");
		return;
	}

	prDev = wlanGetNetDev(prGlueInfo, ucBssIndex);
	if (prDev == NULL) {
		DBGLOG(INIT, ERROR, "prDev null\n");
		return;
	}

	wdev = prDev->ieee80211_ptr;

	if (wdev == NULL) {
		DBGLOG(INIT, ERROR, "wdev null\n");
		return;
	}

	size = sizeof(struct PARAM_HANG_INFO);
	list = kalMemAlloc(size, VIR_MEM_TYPE);
	if (!list) {
		DBGLOG(INIT, ERROR, "alloc list fail\n");
		return;
	}

	kalMemZero(list, size);

	list->id = GRID_HANG_INFO;
	/* len does not include id and len */
	list->len = size - 2;

	pucFwVer = &prGlueInfo->prAdapter->rVerInfo.aucReleaseManifest[0];
	/* Fw version datetime stored in the last 14 bytes */
	if (!pucFwVer) {
		DBGLOG(INIT, ERROR, "pucFwVer get NULL\n");
	}

	u4FwVerLen = kalStrLen(pucFwVer);
	if (u4FwVerLen >= 14) {
		u4Offset = u4FwVerLen - 14;
		kalStrnCpy(list->fwVer, (pucFwVer + u4Offset),
			sizeof(list->fwVer) - 1);
	} else {
		kalStrnCpy(list->fwVer, "Invalid_Version", 15);
	}

	DBGLOG(INIT, INFO, "fwVer=%s, full=%s\n", list->fwVer, pucFwVer);

	kalStrnCpy(list->driverVer, aucDriverVersionStr,
		sizeof(list->driverVer) - 1);

	DBGLOG(INIT, INFO, "driverVer=%s\n", list->driverVer);

	kalStrnCpy(list->cidInfo, cid,
		sizeof(list->cidInfo) - 1);

	DBGLOG(INIT, INFO, "cidInfo=%s\n", list->cidInfo);

	list->hangType = u4Reason;

	DBGLOG(INIT, INFO, "hangType=%d\n", list->hangType);

	kalMemCopy(list->rawData, pcData, u4DataLen);

	DBGLOG(INIT, INFO, "Dump=\n%s\n", list->rawData);

	mtk_cfg80211_vendor_event_generic_response(
		wiphy, wdev, size, (uint8_t *)list);
	kalMemFree(list, VIR_MEM_TYPE, size);

	DBGLOG(INIT, INFO, "glNotifyChipReset end\n");
}

void sysResetTrigger(void)
{
	if (g_u4Memdump) {
		if (g_pucTraces == NULL)
			g_pucTraces = kalMemAlloc(RST_REPORT_DATA_MAX_LEN,
				VIR_MEM_TYPE);

		if (g_pucTraces != NULL)
			glWlanGetTraces(g_pucTraces, RST_REPORT_DATA_MAX_LEN);
		else
			DBGLOG(INIT, ERROR, "Alloc mem failed.\n");
	} else {
		DBGLOG(INIT, INFO, "Skip reset report. Memdump=%u\n",
			g_u4Memdump);
	}
}

void sysHangRecoveryReport(void)
{
	uint32_t u4Reason = 0;
	uint8_t acData[512] = {0};

	if (g_u4Memdump) {
		glGetRstInfo(&u4Reason, acData,
			sizeof(acData));
		glNotifyChipReset(u4Reason, acData,
			sizeof(acData));
	}
}

void sysHangTriggerCollectLogs(void)
{
	if (g_u4Memdump == 3) {
		/* Ensure the coredump file saved when the previous coredump
		 * is completed. Otherwise, there needs a few seconds delay
		 * before triggering KE here to avoid coredump file missing
		 * after reboot.
		 */
		DBGLOG(INIT, WARN,
			"Reset reason=[%d]\n",
			glGetRstReason());
		// Customized requirements need to trigger BUG to collect logs.
		BUG_ON(1);
	}
}

void sysMacAddrOverride(uint8_t *prMacAddr)
{
	DBGLOG(INIT, TRACE,
		"Override=%d\n", fgIsMacAddrOverride);

	if (!fgIsMacAddrOverride)
		return;

	wlanHwAddrToBin(
		aucMacAddrOverride,
		prMacAddr);

	DBGLOG(INIT, TRACE,
		"Init macaddr to " MACSTR ".\n",
		MAC2STR(prMacAddr));
}

#if (CFG_EXT_VERSION > 1)
uint32_t wlanCfgGetUint32Range(struct ADAPTER *prAdapter,
			  const int8_t *pucKey, uint32_t u4ValueDef,
			  uint32_t *pu4MinValue, uint32_t *pu4MaxValue)
{
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;
	uint32_t u4Value, u4ReadValue;
	int32_t u4Ret;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	u4Value = u4ValueDef;
	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, WLAN_CFG_DEFAULT);

	if (prWlanCfgEntry) {
		/* u4Ret = kalStrtoul(prWlanCfgEntry->aucValue, NULL, 0); */
		u4Ret = kalkStrtou32(prWlanCfgEntry->aucValue, 0, &u4ReadValue);
		if (u4Ret) {
			DBGLOG(INIT, LOUD, "parse aucValue error u4Ret=%d\n",
			       u4Ret);
		} else {
			if (pu4MinValue && u4ReadValue < *pu4MinValue)
				goto exit;
			if (pu4MaxValue && u4ReadValue > *pu4MaxValue)
				goto exit;
			u4Value = u4ReadValue;
		}
	}

exit:
	wlanCfgRecordValue(prAdapter, pucKey, u4Value);

	return u4Value;
}

int32_t wlanCfgGetInt32Range(struct ADAPTER *prAdapter,
			const int8_t *pucKey, int32_t i4ValueDef,
			int32_t *pi4MinValue, int32_t *pi4MaxValue)
{
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;
	int32_t i4Value = 0, i4ReadValue = 0;
	int32_t i4Ret = 0;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	i4Value = i4ValueDef;
	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, WLAN_CFG_DEFAULT);

	if (prWlanCfgEntry) {
		/* i4Ret = kalStrtol(prWlanCfgEntry->aucValue, NULL, 0); */
		i4Ret = kalkStrtos32(prWlanCfgEntry->aucValue, 0, &i4ReadValue);
		if (i4Ret) {
			DBGLOG(INIT, LOUD, "parse aucValue error i4Ret=%d\n",
			       i4Ret);
		} else {
			if (pi4MinValue && i4ReadValue < *pi4MinValue)
				goto exit;
			if (pi4MaxValue && i4ReadValue > *pi4MaxValue)
				goto exit;
			i4Value = i4ReadValue;
		}
	}

exit:
	wlanCfgRecordValue(prAdapter, pucKey, (uint32_t)i4Value);

	return i4Value;
}

uint32_t _cfgGetUint32(struct ADAPTER *prAdapter, const int8_t *pucKey,
	uint32_t u4ValueDef)
{
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;
	uint32_t u4Value;
	int32_t u4Ret;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	u4Value = u4ValueDef;

	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, WLAN_CFG_DEFAULT);

	if (prWlanCfgEntry) {
		/* u4Ret = kalStrtoul(prWlanCfgEntry->aucValue, NULL, 0); */
		u4Ret = kalkStrtou32(prWlanCfgEntry->aucValue, 0, &u4Value);
		if (u4Ret)
			DBGLOG(INIT, LOUD, "parse aucValue error u4Ret=%d\n",
			       u4Ret);
	}

	wlanCfgRecordValue(prAdapter, pucKey, u4Value);

	return u4Value;
}
#endif

void sysGetExtCfg(struct ADAPTER *prAdapter)
{
#if (CFG_EXT_ROAMING == 1)
	struct WIFI_VAR *prWifiVar = NULL;
	uint32_t u4MinValue = 0, u4MaxValue = 0;
	int32_t  i4MinValue = 0, i4MaxValue = 0;
#endif

#ifdef CFG_MTK_WIFI_SOC_S5E9925_SUPPORT
	if (!prAdapter)
		return;

	prAdapter->rWifiVar.u4PerfMonTpTh[3] = 120;
	prAdapter->rWifiVar.u4PerfMonTpTh[4] = 160;
	prAdapter->rWifiVar.u4PerfMonTpTh[5] = 200;
	prAdapter->rWifiVar.u4PerfMonTpTh[6] = 450;
	prAdapter->rWifiVar.u4PerfMonTpTh[7] = 800;
	prAdapter->rWifiVar.u4PerfMonTpTh[8] = 1200;
	prAdapter->rWifiVar.u4PerfMonTpTh[9] = 2400;
#endif

#if (CFG_EXT_ROAMING == 1)
	prWifiVar = &prAdapter->rWifiVar;

	u4MinValue = 0;
	u4MaxValue = 100;
	prWifiVar->ucRCMinRoamDetla = (uint8_t) wlanCfgGetUint32Range(
		prAdapter, "RoamCommon_MinRoamDetla", 15,
		&u4MinValue, &u4MaxValue);
	u4MinValue = 0;
	u4MaxValue = 30;
	prWifiVar->ucRCDelta = (uint8_t) wlanCfgGetUint32Range(
		prAdapter, "RoamCommon_Delta", 20,
		&u4MinValue, &u4MaxValue);
	u4MinValue = 0;
	u4MaxValue = 20;
	prWifiVar->ucRIDelta = (uint8_t) wlanCfgGetUint32Range(
		prAdapter, "RoamIdle_Delta", 0,
		&u4MinValue, &u4MaxValue);
	i4MinValue = -127;
	i4MaxValue = -70;
	prWifiVar->cRBMinRssi = (int8_t) wlanCfgGetInt32Range(
		prAdapter, "RoamBeaconLoss_TargetMinRSSI", -75,
		&i4MinValue, &i4MaxValue);
	u4MinValue = 0;
	u4MaxValue = 20;
	prWifiVar->ucRBTMDelta = (uint8_t) wlanCfgGetUint32Range(
		prAdapter, "RoamBTM_Delta", 0,
		&u4MinValue, &u4MaxValue);
	/* WTC Mode */
	prWifiVar->ucScanMode = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamWTC_ScanMode", 1);
	u4MinValue = -90;
	u4MaxValue = -60;
	prWifiVar->cRssiThreshold = (int8_t) wlanCfgGetInt32Range(
		prAdapter, "RoamWTC_HandlingRSSIThreshold", -70,
		&i4MinValue, &i4MaxValue);
	u4MinValue = -90;
	u4MaxValue = -60;
	prWifiVar->cRssiThreshold_24G = (int8_t) wlanCfgGetInt32Range(
		prAdapter, "RoamWTC_24GCandiRSSIThreshold", -70,
		&i4MinValue, &i4MaxValue);
	u4MinValue = -90;
	u4MaxValue = -60;
	prWifiVar->cRssiThreshold_5G = (int8_t) wlanCfgGetInt32Range(
		prAdapter, "RoamWTC_5GCandiRSSIThreshold", -70,
		&i4MinValue, &i4MaxValue);
#if (CFG_SUPPORT_WIFI_6G == 1)
	u4MinValue = -90;
	u4MaxValue = -60;
	prWifiVar->cRssiThreshold_6G = (int8_t) wlanCfgGetInt32Range(
		prAdapter, "RoamWTC_6GCandiRSSIThreshold", -70,
		&i4MinValue, &i4MaxValue);
#endif
	/* BT Coex */
	u4MinValue = 0;
	u4MaxValue = 100;
	prWifiVar->ucRBTCScoreW = (uint8_t) wlanCfgGetUint32Range(
		prAdapter, "RoamBTCoex_ScoreWeight", 70,
		&u4MinValue, &u4MaxValue);
	u4MinValue = 0;
	u4MaxValue = 100;
	prWifiVar->ucRBTCETPW = (uint8_t) wlanCfgGetUint32Range(
		prAdapter, "RoamBTCoex_ETPWeight", 70,
		&u4MinValue, &u4MaxValue);
	i4MinValue = -90;
	i4MaxValue = -60;
	prWifiVar->cRBTCRssi = (int8_t) wlanCfgGetInt32Range(
		prAdapter, "RoamBTCoex_TargetMinRSSI", -75,
		&i4MinValue, &i4MaxValue);
	u4MinValue = 0;
	u4MaxValue = 20;
	prWifiVar->ucRBTCDelta = (uint8_t) wlanCfgGetUint32Range(
		prAdapter, "RoamBTCoex_Delta", 10,
		&u4MinValue, &u4MaxValue);
	/* AP socring */
	u4MinValue = 0;
	u4MaxValue = 100;
	prWifiVar->ucRssiWeight = (uint8_t) wlanCfgGetUint32Range(
		prAdapter, "RoamAPScore_RSSIWeight", 65,
		&u4MinValue, &u4MaxValue);
	u4MinValue = 0;
	u4MaxValue = 100;
	prWifiVar->ucCUWeight = (uint8_t) wlanCfgGetUint32Range(
		prAdapter, "RoamAPScore_CUWeight", 35,
		&u4MinValue, &u4MaxValue);
	i4MinValue = -95;
	i4MaxValue = -45;
	prWifiVar->cConMinRssi = (int8_t) wlanCfgGetInt32Range(
		prAdapter, "ConNonHint_TargetMinRSSI", -75,
		&i4MinValue, &i4MaxValue);
	i4MinValue = 0;
	i4MaxValue = 30;
	prWifiVar->ucRCMloTpPref = (int8_t) wlanCfgGetInt32Range(
		prAdapter, "RoamCommon_Mlo_TpPrefer", 10,
		&i4MinValue, &i4MaxValue);
	prWifiVar->cB1RssiFactorVal1 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorValue1", -55);
	prWifiVar->cB1RssiFactorVal2 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorValue2", -60);
	prWifiVar->cB1RssiFactorVal3 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorValue3", -70);
	prWifiVar->cB1RssiFactorVal4 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorValue4", -80);
	prWifiVar->cB1RssiFactorVal5 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorValue5", -90);
	prWifiVar->cB2RssiFactorVal1 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorValue1", -55);
	prWifiVar->cB2RssiFactorVal2 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorValue2", -60);
	prWifiVar->cB2RssiFactorVal3 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorValue3", -70);
	prWifiVar->cB2RssiFactorVal4 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorValue4", -80);
	prWifiVar->cB2RssiFactorVal5 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorValue5", -90);
	prWifiVar->cB3RssiFactorVal1 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_RSSIFactorValue1", -60);
	prWifiVar->cB3RssiFactorVal2 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_RSSIFactorValue2", -65);
	prWifiVar->cB3RssiFactorVal3 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_RSSIFactorValue3", -80);
	prWifiVar->cB3RssiFactorVal4 = (int8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_RSSIFactorValue4", -90);
	prWifiVar->ucB1RssiFactorScore1 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorScore1", 100);
	prWifiVar->ucB1RssiFactorScore2 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorScore2", 90);
	prWifiVar->ucB1RssiFactorScore3 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorScore3", 60);
	prWifiVar->ucB1RssiFactorScore4 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorScore4", 20);
	prWifiVar->ucB1RssiFactorScore5 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_RSSIFactorScore5", 0);
	prWifiVar->ucB2RssiFactorScore1 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorScore1", 100);
	prWifiVar->ucB2RssiFactorScore2 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorScore2", 90);
	prWifiVar->ucB2RssiFactorScore3 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorScore3", 60);
	prWifiVar->ucB2RssiFactorScore4 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorScore4", 20);
	prWifiVar->ucB2RssiFactorScore5 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_RSSIFactorScore5", 0);
	prWifiVar->ucB3RssiFactorScore1 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_RSSIFactorScore1", 120);
	prWifiVar->ucB3RssiFactorScore2 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_RSSIFactorScore2", 50);
	prWifiVar->ucB3RssiFactorScore3 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_RSSIFactorScore3", 20);
	prWifiVar->ucB3RssiFactorScore4 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_RSSIFactorScore4", 0);
	prWifiVar->ucB1CUFactorVal1 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_CUFactorValue1", 10);
	prWifiVar->ucB1CUFactorVal2 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_CUFactorValue2", 70);
	prWifiVar->ucB2CUFactorVal1 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_CUFactorValue1", 30);
	prWifiVar->ucB2CUFactorVal2 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_CUFactorValue2", 80);
	prWifiVar->ucB3CUFactorVal1 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_CUFactorValue1", 30);
	prWifiVar->ucB3CUFactorVal2 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_CUFactorValue2", 80);
	prWifiVar->ucB1CUFactorScore1 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_CUFactorScore1", 100);
	prWifiVar->ucB1CUFactorScore2 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band1_CUFactorScore2", 20);
	prWifiVar->ucB2CUFactorScore1 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_CUFactorScore1", 100);
	prWifiVar->ucB2CUFactorScore2 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band2_CUFactorScore2", 20);
	prWifiVar->ucB3CUFactorScore1 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_CUFactorScore1", 120);
	prWifiVar->ucB3CUFactorScore2 = (uint8_t) _cfgGetUint32(
		prAdapter, "RoamAPScore_Band3_CUFactorScore2", 20);
#endif
}

/*
 * INI Table Information
 * [Key/Default/Minimum/Maximum/Range-present]
 */
struct INI_TABLE_INFO arIniTable[MAX_TABLE_ENTRY] = {
	/* Connection */
	{"ConBeaconLoss_TimeoutOnWakeUp",	6,	0,	20,	1},
	{"ConBeaconLoss_TimeoutOnSleep",	10,	0,	20,	1},
	{"ConDTIMSkipping_Number", 		3,	0,	10,	1},
	{"ConDTIMSkipping_MaxTime",		500,	0,	2000,	1},
	{"ConKeepAlive_interval", 		30,	0,	120,	1},
	/* Roaming */
	{"RoamCommon_MinRoamDelta", 		15, 	0,	100,	1},
	{"RoamCommon_Delta", 			20, 	0,	30,	1},
	{"RoamCommon_Mlo_TpPrefer",		10,	0,	30,	1},
	{"RoamScan_FirstTimer", 		10, 	0,	20,	1},
	{"RoamScan_InactiveTimer", 		10, 	0,	20,	1},
	{"RoamScan_InactiveCount",		5,	0,	20,	1},
	{"RoamScan_StepRSSI", 			5,	0, 	20,	1},
	{"RoamScan_ActiveCH_DwellTime",		40,	0,	200,	1},
	{"RoamScan_PassiveCH_DwellTime",	130,	0,	200,	1},
	{"RoamScan_HomeTime", 			45,	0,	200,	1},
	{"RoamScan_AwayTime",			100,	0,	200,	1},
	{"RoamRSSI_Trigger", 			-75,	-100,	-50,	1},
	{"RoamCU_Trigger", 			70,	60,	90,	1},
	{"RoamCU_MonitorTime", 			10,	0,	20,	1},
	{"RoamCU_24GRSSIRange", 		-60,	-70,	-50,	1},
	{"RoamCU_5GRSSIRange", 			-70,	-70,	-50,	1},
	{"RoamCU_6GRSSIRange", 			-70,	-70,	-50,	1},
	{"RoamCU_24DefaultCU",			70,	0,	100,	1},
	{"RoamCU_5DefaultCU",			70,	0,	100,	1},
	{"RoamCU_6DefaultCU",			70,	0,	100,	1},
	{"RoamIdle_TriggerBand",		3,	0,	0,	0},
	{"RoamIdle_InactiveTime", 		10,	0,	20,	1},
	{"RoamIdle_MinRSSI",			-60,	-70,	-50,	1},
	{"RoamIdle_RSSIVariation",		5,	0,	10,	1},
	{"RoamIdle_InactivePacketCount", 	5,	0,	20,	1},
	{"RoamIdle_Delta",			0,	0,	20,	1},
	{"RoamBeaconLoss_TargetMinRSSI", 	-75,	-127,	-70,	1},
	{"RoamBTM_Delta", 			0,	0,	20,	1},
	{"RoamWTC_ScanMode", 			1,	0,	0,	0},
	{"RoamWTC_HandlingRSSIThreshold", 	-70,	-90,	-60,	1},
	{"RoamWTC_24GCandiRSSIThreshold", 	-70,	-90,	-60,	1},
	{"RoamWTC_5GCandiRSSIThreshold", 	-70,	-90,	-60,	1},
	{"RoamWTC_6GCandiRSSIThreshold",	-70,	-90,	-60,	1},
	{"RoamBTCoex_ScoreWeight", 		70,	0,	100,	1},
	{"RoamBTCoex_ETPWeight",		70,	0,	100,	1},
	{"RoamBTCoex_TargetMinRSSI",		-75,	-90,	-60,	1},
	{"RoamBTCoex_Delta", 			10,	0,	20,	1},
	{"RoamBTCoex_ThresholdTime",		10,	0,	100,	1},
	/* AP Scoring */
	{"RoamAPScore_RSSIWeight", 		65,	0,	100,	1},
	{"RoamAPScore_CUWeight", 		35, 	0,	100,	1},
	{"RoamAPScore_Band1_RSSIFactorValue1", 	-55,	0,	0,	0},
	{"RoamAPScore_Band1_RSSIFactorScore1", 	100,	0,	0,	0},
	{"RoamAPScore_Band1_RSSIFactorValue2", 	-60,	0,	0,	0},
	{"RoamAPScore_Band1_RSSIFactorScore2", 	90,	0,	0,	0},
	{"RoamAPScore_Band1_RSSIFactorValue3",	-70,	0,	0,	0},
	{"RoamAPScore_Band1_RSSIFactorScore3", 	60,	0,	0,	0},
	{"RoamAPScore_Band1_RSSIFactorValue4", 	-80,	0,	0,	0},
	{"RoamAPScore_Band1_RSSIFactorScore4", 	20,	0,	0,	0},
	{"RoamAPScore_Band1_RSSIFactorValue5", 	-90,	0,	0,	0},
	{"RoamAPScore_Band1_RSSIFactorScore5",	0,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorValue1",	-55,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorScore1",	100,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorValue2",	-60,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorScore2",	90,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorValue3",	-70,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorScore3",	60,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorValue4",	-80,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorScore4",	20,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorValue5", 	-90,	0,	0,	0},
	{"RoamAPScore_Band2_RSSIFactorScore5", 	0,	0,	0,	0},
	{"RoamAPScore_Band3_RSSIFactorValue1",	-60,	0,	0,	0},
	{"RoamAPScore_Band3_RSSIFactorScore1",	120,	0,	0,	0},
	{"RoamAPScore_Band3_RSSIFactorValue2",	-65,	0,	0,	0},
	{"RoamAPScore_Band3_RSSIFactorScore2",	50,	0,	0,	0},
	{"RoamAPScore_Band3_RSSIFactorValue3",	-80,	0,	0,	0},
	{"RoamAPScore_Band3_RSSIFactorScore3",	20,	0,	0,	0},
	{"RoamAPScore_Band3_RSSIFactorValue4",	-90,	0,	0,	0},
	{"RoamAPScore_Band3_RSSIFactorScore4",	0,	0,	0,	0},
	{"RoamAPScore_Band1_CUFactorValue1",	10,	0,	0,	0},
	{"RoamAPScore_Band1_CUFactorScore1", 	100,	0,	0,	0},
	{"RoamAPScore_Band1_CUFactorValue2",	70,	0,	0,	0},
	{"RoamAPScore_Band1_CUFactorScore2",	20,	0,	0,	0},
	{"RoamAPScore_Band2_CUFactorValue1",	30,	0,	0,	0},
	{"RoamAPScore_Band2_CUFactorScore1", 	100,	0,	0,	0},
	{"RoamAPScore_Band2_CUFactorValue2",	80,	0,	0,	0},
	{"RoamAPScore_Band2_CUFactorScore2",	20,	0,	0,	0},
	{"RoamAPScore_Band3_CUFactorValue1",	30, 	0,	0,	0},
	{"RoamAPScore_Band3_CUFactorScore1", 	120,	0,	0,	0},
	{"RoamAPScore_Band3_CUFactorValue2",	80,	0,	0,	0},
	{"RoamAPScore_Band3_CUFactorScore2",	20,	0,	0,	0},
	/* NCHO */
	{"RoamNCHO_Trigger",			-75,	-100,	-50, 	1},
	{"RoamNCHO_Delta", 			10,	0,	30, 	1},
	{"RoamNCHO_FullScanPeriod",		120,	60,	300, 	1},
	{"RoamNCHO_PartialScanPeriod",		10,	0,	20, 	1},
	{"RoamNCHO_ActiveCH_DwellTime",		40,	0,	200, 	1},
	{"RoamNCHO_PassiveCH_DwellTime",	130,	0,	200, 	1},
	{"RoamNCHO_HomeTime", 			45,	0,	200, 	1},
	{"RoamNCHO_AwayTime",			100,	0,	200, 	1},
	{"",0,0,0},
};

struct INI_READ_INFO arReadIni[MAX_READ_ENTRY];

void iniTableParsing (uint32_t *total_num)
{
	int i;

	DBGLOG(INIT, TRACE, "[INI TABLE INFO]");
	DBGLOG(INIT, TRACE, "def  min  max  range [key]");

	for (i = 0; i < MAX_TABLE_ENTRY; i++) {
		if (arIniTable[i].u8Key[0] == '\0') {
			*total_num = i;
			break;
		}
		DBGLOG(INIT, TRACE, "%3d  %3d  %3d  %3d   [%s]",
			arIniTable[i].i8Default,
			arIniTable[i].i8Min, arIniTable[i].i8Max,
			arIniTable[i].fgHasRange, arIniTable[i].u8Key);
	}

	DBGLOG(INIT, TRACE, "[total_num=%d]", *total_num);

	return;
}



void iniFileErrorCheck (struct ADAPTER *prAdapter, uint8_t **ppucIniBuf,
	uint32_t *pu4ReadSize)
{
	uint32_t total_table_num = 0;
	uint8_t fgNeedBackupIni = FALSE;
	uint8_t *pucTempBuf;
	uint32_t ret;

	if (prAdapter == NULL) {
		DBGLOG(INIT, INFO, "\nprAdapter is NULL");
		return;
	}

	iniTableParsing (&total_table_num);

	if (kalRequestFirmware("wlan-connection-roaming.ini", ppucIniBuf,
		   pu4ReadSize, TRUE, prAdapter->prGlueInfo->prDev) == 0) {
		DBGLOG(INIT, INFO, "Read wlan-connection-roaming.ini\n");
		pucTempBuf = kalMemZAlloc(*pu4ReadSize, VIR_MEM_TYPE);
		if (pucTempBuf) {
			kalMemCopy(pucTempBuf, *ppucIniBuf, *pu4ReadSize);
			ret = iniFileParsing (pucTempBuf, total_table_num);
			if (ret == WLAN_STATUS_FAILURE) {
				DBGLOG(INIT, INFO, "ini parsing error");
				kalMemFree(*ppucIniBuf, VIR_MEM_TYPE, *pu4ReadSize);
				*pu4ReadSize = 0;
				*ppucIniBuf = NULL;
				fgNeedBackupIni = TRUE;
			}
			kalMemFree(pucTempBuf, VIR_MEM_TYPE, *pu4ReadSize);
		}
		else
			DBGLOG(INIT, WARN, "alloc pucTempBuf fail");
	}
	else {
		DBGLOG(INIT, INFO, "No wlan-connection-roaming.ini\n");
		fgNeedBackupIni = TRUE;
	}

	if (fgNeedBackupIni) {
		if (kalRequestFirmware("wlan-connection-roaming-backup.ini",
			ppucIniBuf, pu4ReadSize, TRUE, prAdapter->prGlueInfo->prDev) == 0) {
			DBGLOG(INIT, INFO, "Read wlan-connection-roaming-backup.ini\n");
			pucTempBuf = kalMemZAlloc(*pu4ReadSize, VIR_MEM_TYPE);
			if (pucTempBuf) {
				kalMemCopy(pucTempBuf, *ppucIniBuf, *pu4ReadSize);
				ret = iniFileParsing (pucTempBuf, total_table_num);
				if (ret == WLAN_STATUS_FAILURE) {
					DBGLOG(INIT, INFO, "backup ini parsing error");
					kalMemFree(*ppucIniBuf, VIR_MEM_TYPE, *pu4ReadSize);
					*pu4ReadSize = 0;
					*ppucIniBuf = NULL;
				}
				kalMemFree(pucTempBuf, VIR_MEM_TYPE, *pu4ReadSize);
			}
			else
				DBGLOG(INIT, WARN, "alloc pucTempBuf fail");
		}
		else
			DBGLOG(INIT, INFO, "No wlan-connection-roaming-backup.ini\n");
	}

	return;
}

uint32_t iniFileParsing (uint8_t *aucIniText, uint32_t table_num)
{
	struct INI_PARSE_STATE_S state;

	int8_t *apcArgv[INI_ARGV_MAX];
	int8_t **ppcArgs;
	int32_t	i4Nargs;
	int8_t arcArgv_size[INI_ARGV_MAX];
	uint8_t line_count = 0;
	int32_t i4Ret = 0;
	int i, j;

	ppcArgs = apcArgv;
	i4Nargs = 0;
	state.ptr = aucIniText;
	state.nexttoken = 0;
	state.textsize = 0;

	kalMemSet(&arReadIni[0], 0, sizeof(struct INI_READ_INFO) * MAX_READ_ENTRY);

	for (;;) {
		switch(iniFindNextToken(&state)) {
			case INI_STATE_EOF:
				if (i4Nargs < 2)
					goto exit;

				/* 3 parmeter mode transforation */
				if (i4Nargs == 3) {
					DBGLOG(INIT, INFO, "ini error:3 parameters [%s,%s,%s]",
						ppcArgs[0], ppcArgs[1], ppcArgs[2]);
					return WLAN_STATUS_FAILURE;
				}

				goto exit;

			case INI_STATE_NEWLINE:
				if (i4Nargs < 2)
					break;

				/* 3 parmeter mode transforation */
				if (i4Nargs == 3) {
					DBGLOG(INIT, INFO, "ini error:3 parameters [%s,%s,%s]",
						ppcArgs[0], ppcArgs[1], ppcArgs[2]);
					return WLAN_STATUS_FAILURE;
				}

				memset (arcArgv_size, 0, INI_ARGV_MAX);
				memset (apcArgv, 0, INI_ARGV_MAX * sizeof(int8_t *));
				i4Nargs = 0;
				break;

			case INI_STATE_TEXT:
				if (i4Nargs >= 0 && i4Nargs < INI_ARGV_MAX) {
					if (i4Nargs == 0) {
						strcpy (arReadIni[line_count].u8Key, state.text);
					}

					if (i4Nargs == 1) {
						i4Ret = kalkStrtos32(state.text, 0,
							&(arReadIni[line_count].i8Value));
						line_count++;
					}
					ppcArgs[i4Nargs++] = state.text;
					arcArgv_size[i4Nargs - 1] = state.textsize;
					state.textsize = 0;
					DBGLOG(INIT, TRACE, "nargs= %d STATE_TEXT = %s, SIZE = %d",
						   i4Nargs - 1, ppcArgs[i4Nargs - 1],
						   arcArgv_size[i4Nargs - 1]);
				}
				break;

			case INI_STATE_ERROR:
				DBGLOG(INIT, TRACE, "[INI_STATE_ERROR]");
				return WLAN_STATUS_FAILURE;
		}
	}

exit:
	DBGLOG(INIT, TRACE, "[Read INI File]");
	for (i = 0; i < MAX_READ_ENTRY; i++) {
		if (arReadIni[i].u8Key[0] == '\0') {
			break;
		}
		DBGLOG(INIT, TRACE, "%3d [%s]", arReadIni[i].i8Value,
			arReadIni[i].u8Key);
	}

	DBGLOG(INIT, TRACE, "[Check duplicated parameter]");
	for (i = 0; i < MAX_READ_ENTRY; i++) {
		if (arReadIni[i].u8Key[0] == '\0') break;
		for (j = i+1; j < MAX_READ_ENTRY; j++) {
			if (arReadIni[j].u8Key[0] == '\0') break;
			if (kalStrCmp(arReadIni[i].u8Key, arReadIni[j].u8Key) == 0) {
				DBGLOG(INIT, INFO, "ini duplicated parameter error [%s, %d]",
					arReadIni[i].u8Key, arReadIni[i].i8Value);
				return WLAN_STATUS_FAILURE;
			}
		}
	}

	DBGLOG(INIT, TRACE, "[Error checking]");
	for (i = 0; i < MAX_READ_ENTRY; i++) {
		if (arReadIni[i].u8Key[0] == '\0') break;
		for (j = 0; j < MAX_TABLE_ENTRY; j++) {
			if (j == table_num) {
				DBGLOG(INIT, INFO, "ini unsupported key:read=%d [%s]",
					arReadIni[i].i8Value, arReadIni[i].u8Key);
				break;
			}

			if (arIniTable[j].u8Key[0] == '\0') break;

			if (kalStrCmp(arReadIni[i].u8Key, arIniTable[j].u8Key) == 0) {
				if (arIniTable[j].fgHasRange == 1) {
					if (arReadIni[i].i8Value < arIniTable[j].i8Min) {
						DBGLOG(INIT, INFO,
							"ini found:read=%d(%d, %d) [%s] [Out of Range!]",
							arReadIni[i].i8Value, arIniTable[j].i8Min,
							arIniTable[j].i8Max, arReadIni[i].u8Key);
					}
					else if (arReadIni[i].i8Value > arIniTable[j].i8Max) {
						DBGLOG(INIT, INFO,
							"ini found:read=%d(%d, %d) [%s] [Out of Range!]",
							arReadIni[i].i8Value, arIniTable[j].i8Min,
							arIniTable[j].i8Max, arReadIni[i].u8Key);
					}
					else
						DBGLOG(INIT, TRACE,
							"ini found:read=%d(%d, %d) [%s]",
							arReadIni[i].i8Value, arIniTable[j].i8Min,
							arIniTable[j].i8Max, arReadIni[i].u8Key);
				}
				else {
					DBGLOG(INIT, TRACE,
						"ini found:read=%d(%d, %d), [%s] [No Range]",
						arReadIni[i].i8Value, arIniTable[j].i8Min,
						arIniTable[j].i8Max, arReadIni[i].u8Key);
				}
				break;
			}
		}
	}

	return WLAN_STATUS_SUCCESS;
}

int32_t iniFindNextToken(struct INI_PARSE_STATE_S
			     *state)
{
	int8_t *x = state->ptr;
	int8_t *s;

	if (state->nexttoken) {
		int32_t t = state->nexttoken;

		state->nexttoken = 0;
		return t;
	}

	for (;;) {
		switch (*x) {
		case 0:
			state->ptr = x;
			return INI_STATE_EOF;
		case '\n':
			x++;
			state->ptr = x;
			return INI_STATE_NEWLINE;
		case ' ':
		case ',':
		/*case ':':  should not including : , mac addr would be fail*/
		case '\t':
		case '\r':
		case '=':
			x++;
			continue;
		case '#':
		case '[':
			while (*x && (*x != '\n'))
				x++;
			if (*x == '\n') {
				state->ptr = x + 1;
				return INI_STATE_NEWLINE;
			}
			state->ptr = x;
			return INI_STATE_EOF;

		default:
			goto text;
		}
	}

textdone:
	state->ptr = x;
	*s = 0;
	return INI_STATE_TEXT;
text:
	state->text = s = x;
textresume:
	for (;;) {
		switch (*x) {
		case 0:
			goto textdone;
		case ' ':
		case ',':
		case '\t':
		case '\r':
		case '=':
			x++;
			goto textdone;
		case '\n':
			state->nexttoken = INI_STATE_NEWLINE;
			x++;
			goto textdone;
		case '!':
		case '@':
		case '$':
		case '%':
		case '^':
		case '&':
		case '*':
		case '(':
		case ')':
		case '{':
		case '}':
		case '~':
		case ';':
		case ':':
		case '<':
		case '>':
		case '?':
		case '\'':
			DBGLOG(INIT, INFO, "ini wrong character [%c]", *x);
			*s++ = *x++;
			state->textsize++;
			return INI_STATE_ERROR;
		case '"':
			x++;
			for (;;) {
				switch (*x) {
				case 0:
					/* unterminated quoted thing */
					state->ptr = x;
					return INI_STATE_EOF;
				case '"':
					x++;
					goto textresume;
				default:
					*s++ = *x++;
				}
			}
			break;
		case '\\':
			x++;
			switch (*x) {
			case 0:
				goto textdone;
			case 'n':
				*s++ = '\n';
				break;
			case 'r':
				*s++ = '\r';
				break;
			case 't':
				*s++ = '\t';
				break;
			case '\\':
				*s++ = '\\';
				break;
			case '\r':
				/* \ <cr> <lf> -> line continuation */
				if (x[1] != '\n') {
					x++;
					continue;
				}
				kal_fallthrough;
			case '\n':
				/* \ <lf> -> line continuation */
				x++;
				/* eat any extra whitespace */
				while ((*x == ' ') || (*x == '\t'))
					x++;
				continue;
			default:
				/* unknown escape -- just copy */
				*s++ = *x++;
			}
			continue;
		default:
			*s++ = *x++;
			state->textsize++;
		}
	}
	return INI_STATE_EOF;
}

#endif

