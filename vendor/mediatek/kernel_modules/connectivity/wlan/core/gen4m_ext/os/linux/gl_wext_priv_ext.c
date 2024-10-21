/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /os/linux/gl_wext_priv_ext.c
 */

/*! \file   "gl_wext_priv_ext.c"
 *  \brief This file includes private ioctl support.
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
#include "gl_wext_priv.h"
#include "gl_cmd_validate.h"
#if CFG_ENABLE_WIFI_DIRECT
#include "gl_p2p_os.h"
#endif
#if CFG_SUPPORT_NAN
#include "nan_ext_eht.h"
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

#define CMD_GET_STA_DUMP		"GET_STA_DUMP"

/* SAP Command */
#define CMD_SET_HAPD_AXMODE        "HAPD_SET_AX_MODE"
#define GET_HOTSPOT_ANTENNA_MODE   "GET_HOTSPOT_ANTENNA_MODE"
#define SET_HOTSPOT_ANTENNA_MODE   "SET_HOTSPOT_ANTENNA_MODE"
#define CMD_FORCE_SAP_RPS       "FORCE_SAP_RPS"
#define CMD_FORCE_SAP_SUS       "FORCE_SAP_SUS"
#define CMD_SET_SAP_SUS       "SET_AP_SUSPEND"
#define CMD_ENABLE_SAP_RPS      "SET_AP_RPS"
#define CMD_SET_SAP_RPS_PARAM   "SET_AP_RPS_PARAMS"
#define CMD_QUERY_RPS	"QUERY_RPS"

#define SAP_RPS_PHASE_MIN		(1)
#define SAP_RPS_PHASE_MAX		(10)
#define SAP_RPS_STATUS_TRUE		(1)
#define SAP_RPS_STATUS_FALSE		(0)
#define SAP_RPS_IPKT_MAX		(1000)
#define SAP_RPS_THREHOLD_MAX		(120)
#define SAP_RPS_ENABLE_TRUE		(1)

/* P2P Command */
#define CMD_ECSA			"ECSA"

/* TDLS Command */
#define CMD_GET_TDLS_AVAILABLE "GET_TDLS_AVAILABLE"
#define CMD_GET_TDLS_WIDER_BW "GET_TDLS_WIDER_BW"
#define CMD_GET_TDLS_MAX_SESSION "GET_TDLS_MAX_SESSION"
#define CMD_GET_TDLS_NUM_OF_SESSION "GET_TDLS_NUM_OF_SESSION"
#define CMD_SET_TDLS_ENABLED "SET_TDLS_ENABLED"

/* IDC Command */
#if CFG_SUPPORT_IDC_CH_SWITCH
#define CMD_SET_IDC_RIL		"SET_RIL_BRIDGE"
#endif

/* UWB Command */
#define CMD_SET_UWBCX_ENABLE	"SET_UWBCX_ENABLE"
#define CMD_GET_UWBCX_ENABLE	"GET_UWBCX_ENABLE"
#define CMD_SET_UWBCX_PREPARE_TIME		"SET_UWBCX_PREPARE_TIME"
#define CMD_GET_UWBCX_PREPARE_TIME		"GET_UWBCX_PREPARE_TIME"

/* LLW Command */
#define CMD_SET_INDOOR_CHANNELS		"SET_INDOOR_CHANNELS"
#define CMD_LATENCY_CRT_DATA_SET		"SET_LATENCY_CRT_DATA"
#define CMD_DWELL_TIME_SET			"SET_DWELL_TIME"
#if CFG_SUPPORT_MANIPULATE_TID
#define CMD_MANIPULATE_TID_SET			"SET_TID"
#endif

/* Log Command */
#define CMD_DEBUG_LEVEL_SET			"SET_DEBUG_LEVEL"
#define CMD_BCN_RECV_START "BEACON_RECV start"
#define CMD_BCN_RECV_STOP "BEACON_RECV stop"

/* Bigdata Command */
#define CMD_GET_BSSINFO "GETBSSINFO"
#define CMD_GET_STAINFO "GETSTAINFO"
#define CMD_GET_STA_TRX_INFO "GETSTAINFO"
#define CMD_GET_CONN_REJECT_INFO "GETASSOCREJECTINFO"

#if CFG_SUPPORT_BW_SELECT
#define CMD_SET_MAX_BW "SET_MAX_BW"
#define CMD_GET_MAX_BW "GET_MAX_BW"
#define LEAKY_AP_DETECT 1
#endif

/* TWT Command */
#define CMD_TWT_SETUP "TWT_SETUP"
#define CMD_TWT_TEARDOWN "TWT_TEARDOWN"
#define CMD_GET_TWT_CAP "GET_TWT_CAP"
#define CMD_GET_TWT_STATUS "GET_TWT_STATUS"

#define CMD_SCHED_PM_SETUP "SCHED_PM_SETUP"
#define CMD_SCHED_PM_TEARDOWN "SCHED_PM_TEARDOWN"
#define CMD_GET_SCHED_PM_STATUS "GET_SCHED_PM_STATUS"
#define CMD_SCHED_PM_SUSPEND "SCHED_PM_SUSPEND"
#define CMD_SCHED_PM_RESUME "SCHED_PM_RESUME"
#define CMD_LEAKY_AP_ACTIVE_DETECT "LEAKY_AP_ACTIVE_DETECTION"
#define CMD_LEAKY_AP_PASSIVE_DETECT_START "LEAKY_AP_PASSIVE_DETECTION_START"
#define CMD_LEAKY_AP_PASSIVE_DETECT_END "LEAKY_AP_PASSIVE_DETECTION_END"
#define CMD_LEAKY_AP_GRACE_PERIOD "SET_GRACE_PERIOD"

#define CMD_SET_DELAYED_WAKEUP "SET_DELAYED_WAKEUP"
#define CMD_SET_DELAYED_WAKEUP_TYPE "SET_DELAYED_WAKEUP_TYPE"

/* NCHO related command definition. Setting by supplicant */
#define CMD_NCHO_ROAM_TRIGGER_GET		"GETROAMTRIGGER"
#define CMD_NCHO_ROAM_TRIGGER_SET		"SETROAMTRIGGER"
#define CMD_NCHO_ROAM_DELTA_GET			"GETROAMDELTA"
#define CMD_NCHO_ROAM_DELTA_SET			"SETROAMDELTA"
#define CMD_NCHO_ROAM_SCAN_PERIOD_GET		"GETROAMSCANPERIOD"
#define CMD_NCHO_ROAM_SCAN_PERIOD_SET		"SETROAMSCANPERIOD"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_GET		"GETROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_SET		"SETROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_ADD		"ADDROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CONTROL_GET		"GETROAMSCANCONTROL"
#define CMD_NCHO_ROAM_SCAN_CONTROL_SET		"SETROAMSCANCONTROL"
#define CMD_NCHO_MODE_SET			"SETNCHOMODE"
#define CMD_NCHO_MODE_GET			"GETNCHOMODE"
#define CMD_NCHO_ROAM_SCAN_FREQS_GET		"GETROAMSCANFREQUENCIES"
#define CMD_NCHO_ROAM_SCAN_FREQS_SET		"SETROAMSCANFREQUENCIES"
#define CMD_NCHO_ROAM_SCAN_FREQS_ADD		"ADDROAMSCANFREQUENCIES"
#define CMD_NCHO_ROAM_BAND_GET			"GETROAMBAND"
#define CMD_NCHO_ROAM_BAND_SET			"SETROAMBAND"
#define CMD_GET_CU "GET_CU"
#define CMD_SET_DISCONNECT_IES "SET_DISCONNECT_IES"
#define CMD_GET_ROAMING_REASON_ENABLED "GET_ROAMING_REASON_ENABLED"
#define CMD_SET_ROAMING_REASON_ENABLED "SET_ROAMING_REASON_ENABLED"
#define CMD_GET_BR_ERR_REASON_ENABLED "GET_BR_ERR_REASON_ENABLED"
#define CMD_SET_BR_ERR_REASON_ENABLED "SET_BR_ERR_REASON_ENABLED"
#define CMD_GETBAND		"GETBAND"

#define CMD_ADDROAMSCANCHANNELS_LEGACY "ADDROAMSCANCHANNELS_LEGACY"
#define CMD_GETROAMSCANCHANNELS_LEGACY "GETROAMSCANCHANNELS_LEGACY"
#define CMD_GETROAMSCANFREQUENCIES_LEGACY	"GETROAMSCANFREQUENCIES_LEGACY"
#define CMD_ADDROAMSCANFREQUENCIES_LEGACY	"ADDROAMSCANFREQUENCIES_LEGACY"

#define CMD_REASSOC_FREQUENCY_LEGACY "REASSOC_FREQUENCY_LEGACY"
#define CMD_REASSOC_FREQUENCY "REASSOC_FREQUENCY"
#define CMD_REASSOC_LEGACY "REASSOC_LEGACY"
#define CMD_REASSOC2 "REASSOC"

#define CMD_SETROAMTRIGGER_LEGACY "SETROAMTRIGGER_LEGACY"
#define CMD_GETROAMTRIGGER_LEGACY "GETROAMTRIGGER_LEGACY"

#define CMD_SET_WTC_MODE			"SETWTCMODE"

#define CMD_SETWSECINFO "SETWSECINFO"
#define CMD_SET_FCC_CHANNEL "SET_FCC_CHANNEL"

#if (CFG_SUPPORT_802_11BE_MLO == 1)
#define CMD_SET_ML "ML_LINK_STATE_CONTROL"
#define CMD_GET_ML "GET_ML_LINK_STATE"
#define CMD_SET_WIFI7_ENABLE "SET_WIFI7_ENABLE"
#define CMD_SET_NUM_ALLOWED_MLO_LINK "SET_NUM_ALLOWED_MLO_LINK"
#define CMD_MEASURE_ML_CHANNEL "MEASURE_ML_CHANNEL_CONDITION"
#endif /* CFG_SUPPORT_802_11BE_MLO */
#define CMD_SET_TX_ANT_MODE			"SET_STA_TX_ANTENNA_MODE"
#define CMD_GET_TX_ANT_MODE			"GET_STA_TX_ANTENNA_MODE"
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

/* SAP Command Policy */
struct CMD_VALIDATE_POLICY set_ext_policy[COMMON_CMD_SET_ARG_NUM(2)] = {
	[COMMON_CMD_ATTR_IDX(1)] = {.type = NLA_U8, .min = 0, .max = 8}
};
#if CFG_SAP_RPS_SUPPORT
struct CMD_VALIDATE_POLICY
	rps_enable_policy[COMMON_CMD_SET_ARG_NUM(3)] = {
	[COMMON_CMD_ATTR_IDX(1)] =
		{.type = NLA_U8, .min = 0, .max = SAP_RPS_ENABLE_TRUE},
	[COMMON_CMD_ATTR_IDX(2)] =
		{.type = NLA_STRING, .min = 5, .max = 6}
};
struct CMD_VALIDATE_POLICY
	rps_param_policy[COMMON_CMD_SET_ARG_NUM(6)] = {
	[COMMON_CMD_ATTR_IDX(1)] =
		{.type = NLA_U32, .min = 0, .max = SAP_RPS_IPKT_MAX},
	[COMMON_CMD_ATTR_IDX(2)] =
		{.type = NLA_U8, .min = SAP_RPS_PHASE_MIN, .max = SAP_RPS_PHASE_MAX},
	[COMMON_CMD_ATTR_IDX(3)] =
		{.type = NLA_U32, .min = 0, .max = SAP_RPS_THREHOLD_MAX},
	[COMMON_CMD_ATTR_IDX(4)] =
		{.type = NLA_U8, .min = 0, .max = SAP_RPS_ENABLE_TRUE},
	[COMMON_CMD_ATTR_IDX(5)] =
		{.type = NLA_STRING, .min = 5, .max = 6}
};

struct CMD_VALIDATE_POLICY
	rps_force_set_policy[COMMON_CMD_SET_ARG_NUM(3)] = {
	[COMMON_CMD_ATTR_IDX(1)] =
		{.type = NLA_U8, .min = 0, .max = SAP_RPS_ENABLE_TRUE},
	[COMMON_CMD_ATTR_IDX(2)] =
		{.type = NLA_U8, .min = SAP_RPS_PHASE_MIN, .max = SAP_RPS_PHASE_MAX}
};
#endif
#if CFG_SAP_SUS_SUPPORT
struct CMD_VALIDATE_POLICY sap_sus_policy[COMMON_CMD_SET_ARG_NUM(3)] = {
	[COMMON_CMD_ATTR_IDX(1)] =
		{.type = NLA_U8, .min = 0, .max = SAP_RPS_ENABLE_TRUE},
	[COMMON_CMD_ATTR_IDX(2)] =
		{.type = NLA_STRING, .min = 5, .max = 6}
};
#endif

/* P2P Command Policy */
struct CMD_VALIDATE_POLICY set_ecsa_policy[COMMON_CMD_SET_ARG_NUM(3)] = {
	[COMMON_CMD_ATTR_IDX(1)] = {.type = NLA_U8, .min = 0, .max = U8_MAX},
	[COMMON_CMD_ATTR_IDX(2)] = {.type = NLA_U8, .min = 0, .max = U32_MAX}
};

struct CMD_VALIDATE_POLICY u32_ext_policy[COMMON_CMD_SET_ARG_NUM(7)] = {
	[COMMON_CMD_ATTR_IDX(1)] = {.type = NLA_U32, .min = 0, .max = U32_MAX},
	[COMMON_CMD_ATTR_IDX(2)] = {.type = NLA_U32, .min = 0, .max = U32_MAX},
	[COMMON_CMD_ATTR_IDX(3)] = {.type = NLA_U32, .min = 0, .max = U32_MAX},
	[COMMON_CMD_ATTR_IDX(4)] = {.type = NLA_U32, .min = 0, .max = U32_MAX},
	[COMMON_CMD_ATTR_IDX(5)] = {.type = NLA_U32, .min = 0, .max = U32_MAX},
	[COMMON_CMD_ATTR_IDX(6)] = {.type = NLA_U32, .min = 0, .max = U32_MAX}
};

struct CMD_VALIDATE_POLICY u8_ext_policy[COMMON_CMD_SET_ARG_NUM(7)] = {
	[COMMON_CMD_ATTR_IDX(1)] = {.type = NLA_U8, .min = 0, .max = U8_MAX},
	[COMMON_CMD_ATTR_IDX(2)] = {.type = NLA_U8, .min = 0, .max = U8_MAX},
	[COMMON_CMD_ATTR_IDX(3)] = {.type = NLA_U8, .min = 0, .max = U8_MAX},
	[COMMON_CMD_ATTR_IDX(4)] = {.type = NLA_U8, .min = 0, .max = U8_MAX},
	[COMMON_CMD_ATTR_IDX(5)] = {.type = NLA_U8, .min = 0, .max = U8_MAX},
	[COMMON_CMD_ATTR_IDX(6)] = {.type = NLA_U8, .min = 0, .max = U8_MAX}
};

#if CFG_STAINFO_FEATURE
struct CMD_VALIDATE_POLICY sta_info_policy[COMMON_CMD_SET_ARG_NUM(2)] = {
	[COMMON_CMD_ATTR_IDX(1)] = {.type = NLA_STRING, .min = 3, .max = 17}
};
#endif

#if CFG_SUPPORT_BW_SELECT
struct CMD_VALIDATE_POLICY set_max_bw_policy[COMMON_CMD_SET_ARG_NUM(3)] = {
	[COMMON_CMD_ATTR_IDX(1)] = {.type = NLA_U8, .min = 1, .max = 1},
	[COMMON_CMD_ATTR_IDX(2)] = {.type = NLA_U8, .min = 0, .max = 5}
};
#endif

struct CMD_VALIDATE_POLICY reassoc_ext_policy[COMMON_CMD_SET_ARG_NUM(4)] = {
	[COMMON_CMD_ATTR_IDX(1)] = {.type = NLA_STRING, .len = 17},
	[COMMON_CMD_ATTR_IDX(2)] = {.type = NLA_U32, .min = 0, .max = U32_MAX},
	[COMMON_CMD_ATTR_IDX(3)] = {.type = NLA_U16, .min = 0, .max = U16_MAX}
};

struct CMD_VALIDATE_POLICY set_roam_trigger_ext_policy[COMMON_CMD_SET_ARG_NUM(2)] = {
	[COMMON_CMD_ATTR_IDX(1)] = {.type = NLA_S8, .min = -100, .max = -50},
};

static struct CFG_NCHO_SCAN_CHNL rRoamScnChnl;

static const uint32_t arBwCfg80211Table[] = {
	RATE_INFO_BW_20,
	RATE_INFO_BW_40,
	RATE_INFO_BW_80,
	RATE_INFO_BW_160,
#if (CFG_MTK_ANDROID_WMT == 1 && \
		KERNEL_VERSION(5, 15, 0) <= CFG80211_VERSION_CODE) || \
	KERNEL_VERSION(5, 18, 0) <= CFG80211_VERSION_CODE
	RATE_INFO_BW_320
#endif
};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#ifdef CFG_EXT_FEATURE
#if CFG_EXT_FEATURE

#if CFG_STAINFO_FEATURE
#if (CFG_EXT_VERSION > 2)
static void _updateApRec(struct ADAPTER *prAdapter,
			struct STATS_LLS_PEER_INFO *dst_peer)
{
	struct STATS_LLS_PEER_AP_REC *prPeerApRec = NULL;
	uint8_t i;
	uint8_t j;

	for (i = 0; i < KAL_AIS_NUM; i++) {
		for (j = 0; j < MLD_LINK_MAX; j++) {
			prPeerApRec = &prAdapter->rPeerApRec[i][j];

			if (UNEQUAL_MAC_ADDR(dst_peer->peer_mac_address,
					     prPeerApRec->mac_addr))
				continue;
			dst_peer->bssload.sta_count = prPeerApRec->sta_count;
			dst_peer->bssload.chan_util = prPeerApRec->chan_util;
		}
	}
}
#endif

int32_t mtk_cfg80211_inspect_mac_addr(char *pcMacAddr)
{
	int32_t i = 0;

	if (pcMacAddr == NULL)
		return -1;

	for (i = 0; i < 17; i++) {
		if ((i % 3 != 2) && (!kalIsXdigit(pcMacAddr[i]))) {
			DBGLOG(REQ, ERROR, "[%c] is not hex digit\n",
			       pcMacAddr[i]);
			return -1;
		}
		if ((i % 3 == 2) && (pcMacAddr[i] != ':')) {
			DBGLOG(REQ, ERROR, "[%c]separate symbol is error\n",
			       pcMacAddr[i]);
			return -1;
		}
	}

	if (pcMacAddr[17] != '\0') {
		DBGLOG(REQ, ERROR, "no null-terminated character\n");
		return -1;
	}

	return 0;
}

uint32_t p2pFuncFillCsaInfo(uint8_t *dst, struct HAL_LLS_FW_REPORT *src,
		struct ADAPTER *prAdapter)
{
	struct STATS_LLS_WIFI_IFACE_STAT *iface;

	kalMemCopyFromIo(dst, src, sizeof(struct STATS_LLS_WIFI_IFACE_STAT));
	iface = (struct STATS_LLS_WIFI_IFACE_STAT *)dst;
	DBGLOG(REQ, WARN, "ap peer num: %d\n", iface->num_peers);

	return 0;
}

uint8_t p2pFuncIsValidRateCsa(struct STATS_LLS_RATE_STAT *rate_stats,
	uint32_t ofdm_idx, uint32_t cck_idx)
{
	struct STATS_LLS_WIFI_RATE *rate = &rate_stats->rate;

	switch (rate->preamble) {
	case LLS_MODE_OFDM:
		if (rate->nss >= 1 ||
		    rate->bw >= STATS_LLS_MAX_OFDM_BW_NUM ||
		    ofdm_idx >= STATS_LLS_OFDM_NUM ||
		    rate->rateMcsIdx >= STATS_LLS_HT_NUM)
			goto invalid_rate;
		break;
	case LLS_MODE_CCK:
		if (rate->nss >= 1 ||
		    rate->bw >= STATS_LLS_MAX_CCK_BW_NUM ||
		    cck_idx >= STATS_LLS_CCK_NUM ||
		    rate->rateMcsIdx >= STATS_LLS_HT_NUM)
			goto invalid_rate;
		break;
	case LLS_MODE_HT:
		if (rate->nss >= 1 ||
		    rate->bw >= STATS_LLS_MAX_HT_BW_NUM ||
		    rate->rateMcsIdx >= STATS_LLS_HT_NUM)
			goto invalid_rate;
		break;
	case LLS_MODE_VHT:
		if (rate->nss >= STATS_LLS_MAX_NSS_NUM ||
		    rate->bw >= STATS_LLS_MAX_VHT_BW_NUM ||
		    rate->rateMcsIdx >= STATS_LLS_VHT_NUM)
			goto invalid_rate;
		break;
	case LLS_MODE_HE:
		if (rate->nss >= STATS_LLS_MAX_NSS_NUM ||
		    rate->bw >= STATS_LLS_MAX_HE_BW_NUM ||
		    rate->rateMcsIdx >= STATS_LLS_HE_NUM)
			goto invalid_rate;
		break;
	default:
		goto invalid_rate;
	}
	return TRUE;

invalid_rate:
	DBGLOG(REQ, ERROR, "BAD:preamble=%u nss=%u bw=%u mcs=%u ofdm=%u cck=%u",
			rate->preamble, rate->nss, rate->bw, rate->rateMcsIdx,
			ofdm_idx, cck_idx);
	return FALSE;
}

struct STA_RECORD *p2pFuncFindPeerStarecCsa(struct ADAPTER *prAdapter,
		struct STATS_LLS_PEER_INFO *peer_info)
{
	struct STA_RECORD *prStaRec;
	uint8_t pucIndex;
	uint8_t ucIdx;

	if (!wlanGetWlanIdxByAddress(prAdapter, peer_info->peer_mac_address,
				&pucIndex))
		return NULL;

	for (ucIdx = 0; ucIdx < CFG_STA_REC_NUM; ucIdx++) {
		prStaRec = cnmGetStaRecByIndex(prAdapter, ucIdx);
		if (prStaRec && prStaRec->ucWlanIndex == pucIndex)
			break;
	}

	if (ucIdx == CFG_STA_REC_NUM)
		return NULL;

	return prStaRec;
}

uint32_t p2pFuncFillPeerInfoCsa(uint8_t *dst, struct PEER_INFO_RATE_STAT *src,
		uint32_t *num_peers, struct ADAPTER *prAdapter)
{
	struct STATS_LLS_PEER_INFO *dst_peer;
	struct STATS_LLS_PEER_INFO *src_peer;
	struct STATS_LLS_RATE_STAT *dst_rate;
	struct STATS_LLS_RATE_STAT *src_rate;
	struct STATS_LLS_RATE_STAT temp_state;

	struct STA_RECORD *sta_rec;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
	uint32_t i;
	uint32_t j;

	uint8_t *orig = dst;
	int32_t ofdm_idx;
	int32_t cck_idx;

	*num_peers = 0;
	for (i = 0; i < CFG_STA_REC_NUM; i++, src++) {
		src_peer = &src->peer;
		if (src_peer->type >= STATS_LLS_WIFI_PEER_INVALID)
			continue;

		if (prWifiVar->fgLinkStatsDump)
			DBGLOG(REQ, INFO, "Peer=%u type=%u", i, src_peer->type);

		(*num_peers)++;
		dst_peer = (struct STATS_LLS_PEER_INFO *)dst;

		kalMemCopyFromIo(dst_peer, src_peer,
				sizeof(struct STATS_LLS_PEER_INFO));

		DBGLOG(REQ, WARN, "Peer MAC: " MACSTR,
				MAC2STR(dst_peer->peer_mac_address));
		DBGLOG(REQ, WARN, "src_peer rate_num %u", src_peer->num_rate);
		sta_rec = p2pFuncFindPeerStarecCsa(prAdapter, dst_peer);
		if (sta_rec == NULL && prWifiVar->fgLinkStatsDump) {
			DBGLOG(REQ, WARN, "MAC not found: " MACSTR,
					MAC2STR(dst_peer->peer_mac_address));
		}
		if (src_peer->type == STATS_LLS_WIFI_PEER_AP) {
#if (CFG_EXT_VERSION > 2)
			_updateApRec(prAdapter, dst_peer);
#else
			struct STATS_LLS_PEER_AP_REC *prPeerApRec = NULL;

			for (j = 0, prPeerApRec = prAdapter->rPeerApRec;
					j < KAL_AIS_NUM; j++, prPeerApRec++) {
				if (UNEQUAL_MAC_ADDR(dst_peer->peer_mac_address,
						     prPeerApRec->mac_addr))
					continue;
				dst_peer->bssload.sta_count =
							prPeerApRec->sta_count;
				dst_peer->bssload.chan_util =
							prPeerApRec->chan_util;
			}
#endif
		}

		dst += sizeof(struct STATS_LLS_PEER_INFO);

		dst_peer->num_rate = 0;
		dst_rate = (struct STATS_LLS_RATE_STAT *)dst;
		src_rate = src->rate;
		ofdm_idx = -1;
		cck_idx = -1;
		prAdapter->rQueryStaInfo.u2TxPkts = 0;
		prAdapter->rQueryStaInfo.u2TxRetryPkts = 0;
		prAdapter->rQueryStaInfo.u2TxFailPkts = 0;
		kalMemSet(prAdapter->rQueryStaInfo.u2TxRate,
			0,
			sizeof(prAdapter->rQueryStaInfo.u2TxRate));

		for (j = 0; j < STATS_LLS_RATE_NUM; j++, src_rate++) {
			if (unlikely(src_rate->rate.preamble == LLS_MODE_OFDM))
				ofdm_idx++;
			if (unlikely(src_rate->rate.preamble == LLS_MODE_CCK))
				cck_idx++;

			if (!p2pFuncIsValidRateCsa(src_rate, ofdm_idx, cck_idx))
				continue;

			if (src_rate->tx_mpdu || src_rate->mpdu_lost ||
			    src_rate->retries ) {
				dst_peer->num_rate++;
				DBGLOG(REQ, WARN, "preamble %u", src_rate->rate.preamble);
				DBGLOG(REQ, WARN, "rateMcsIdx %u", src_rate->rate.rateMcsIdx);
				DBGLOG(REQ, WARN, "tx_mpdu %u", src_rate->tx_mpdu);
				DBGLOG(REQ, WARN, "mpdu_lost %u", src_rate->mpdu_lost);
				DBGLOG(REQ, WARN, "retries %u", src_rate->retries);
				prAdapter->rQueryStaInfo.u2TxRate[src_rate->rate.rateMcsIdx] +=
					(src_rate->tx_mpdu + src_rate->retries);
				prAdapter->rQueryStaInfo.u2TxPkts +=
					(src_rate->tx_mpdu + src_rate->retries);
				prAdapter->rQueryStaInfo.u2TxRetryPkts +=
					src_rate->retries;
				prAdapter->rQueryStaInfo.u2TxFailPkts +=
					src_rate->mpdu_lost;

				DBGLOG(REQ, WARN, "trx tx rate %u: %u",
					src_rate->rate.rateMcsIdx,
					prAdapter->rQueryStaInfo.u2TxRate[src_rate->rate.rateMcsIdx]);

				DBGLOG(REQ, WARN, "trx tx_mpdu %u",
					prAdapter->rQueryStaInfo.u2TxPkts);
				DBGLOG(REQ, WARN, "trx mpdu_lost %u",
					prAdapter->rQueryStaInfo.u2TxRetryPkts);
				DBGLOG(REQ, WARN, "trx retries %u",
					prAdapter->rQueryStaInfo.u2TxFailPkts);

				kalMemCopyFromIo(&temp_state, src_rate,
					sizeof(struct STATS_LLS_RATE_STAT));

				dst_rate++;
			}
		}
		dst += sizeof(struct STATS_LLS_RATE_STAT) * dst_peer->num_rate;
		DBGLOG(REQ, WARN, "peer[%u] contains %u rates",
				i, dst_peer->num_rate);

	}

	DBGLOG(REQ, TRACE, "advanced %u bytes", dst - orig);
	return dst - orig;
}

uint32_t p2pFuncFillIfaceCsa(uint8_t *dst, struct HAL_LLS_FW_REPORT *src,
		struct ADAPTER *prAdapter)
{
	struct STATS_LLS_WIFI_IFACE_STAT *iface;
	uint8_t *orig = dst;
	struct BSS_INFO *prBssInfo = NULL;

	prBssInfo = cnmGetSapBssInfo(prAdapter);
	if (!prBssInfo)
		return 0;

	kalMemCopyFromIo(dst, src, sizeof(struct STATS_LLS_WIFI_IFACE_STAT));
	iface = (struct STATS_LLS_WIFI_IFACE_STAT *)dst;
#if CFG_SUPPORT_LLS
	iface->ac[STATS_LLS_WIFI_AC_VO].rx_mpdu =
				prBssInfo->u4RxMpduAc[STATS_LLS_WIFI_AC_VO];
	iface->ac[STATS_LLS_WIFI_AC_VI].rx_mpdu =
				prBssInfo->u4RxMpduAc[STATS_LLS_WIFI_AC_VI];
	iface->ac[STATS_LLS_WIFI_AC_BE].rx_mpdu =
				prBssInfo->u4RxMpduAc[STATS_LLS_WIFI_AC_BE];
	iface->ac[STATS_LLS_WIFI_AC_BK].rx_mpdu =
				prBssInfo->u4RxMpduAc[STATS_LLS_WIFI_AC_BK];
#endif
	dst += sizeof(struct STATS_LLS_WIFI_IFACE_STAT);

	dst += p2pFuncFillPeerInfoCsa(dst, src->peer_info,
			&iface->num_peers, prAdapter);

	DBGLOG(REQ, TRACE, "advanced %u bytes, %u peers",
			dst - orig, iface->num_peers);
	return dst - orig;
}

/**
 * STATS_LLS_WIFI_RADIO_STAT[] <-- 2
 *     ...
 *     num_channels
 *     STATS_LLS_CHANNEL_STAT[] <-- up to 46 (2.4 + 5G; 6G will be more)
 */
uint32_t p2pFuncFillRadioCsa(uint8_t *dst, struct WIFI_RADIO_CHANNEL_STAT *src,
		uint32_t num_radio, struct ADAPTER *prAdapter)
{
	struct STATS_LLS_WIFI_RADIO_STAT *radio;
	struct STATS_LLS_CHANNEL_STAT *src_ch;
	struct STATS_LLS_CHANNEL_STAT *dst_ch;
	struct STATS_LLS_CHANNEL_STAT temp_ch;
	uint8_t *orig = dst;
	uint32_t i, j;
	//enum ENUM_BAND eSapBand = BAND_NULL;
	//struct RF_CHANNEL_INFO rRfChnlInfo;
	struct BSS_INFO *prBssInfo = NULL;

	prBssInfo = cnmGetSapBssInfo(prAdapter);
	if (!prBssInfo)
		return 0;

	for (i = 0; i < num_radio; i++, src++) {
		kalMemCopyFromIo(dst, src,
				sizeof(struct STATS_LLS_WIFI_RADIO_STAT));
		radio = (struct STATS_LLS_WIFI_RADIO_STAT *)dst;
		dst += sizeof(struct STATS_LLS_WIFI_RADIO_STAT);

		radio->num_channels = 0;
		src_ch = src->channel;
		dst_ch = (struct STATS_LLS_CHANNEL_STAT *)dst;
		for (j = 0; j < STATS_LLS_CH_NUM; j++, src_ch++) {
			if (!src_ch->channel.center_freq ||
				(!src_ch->on_time && !src_ch->cca_busy_time))
				continue;
			radio->num_channels++;
			kalMemCopyFromIo(&temp_ch, src_ch, sizeof(*dst_ch));

			DBGLOG(REQ, WARN, "freq:%u, on_time:%u, busy_time:%u",
				temp_ch.channel.center_freq,
				temp_ch.on_time, src_ch->cca_busy_time);

			dst_ch++;
		}

		dst += sizeof(struct STATS_LLS_CHANNEL_STAT) *
			radio->num_channels;

		DBGLOG(REQ, TRACE, "radio[%u] contains %u channels",
				i, radio->num_channels);
	}

	DBGLOG(REQ, TRACE, "advanced %u bytes", dst - orig);
	return dst - orig;
}

void p2pFuncInServiceCsa(
		struct ADAPTER *prAdapter)
{
	int32_t rStatus = 0;

	union {
		struct CMD_GET_STATS_LLS cmd;
		struct EVENT_STATS_LLS_DATA data;
	} query = {0};

	uint32_t u4QueryBufLen = sizeof(query);
	uint32_t u4QueryInfoLen = sizeof(query.cmd);
	uint8_t *buf = NULL;
	uint8_t *ptr = NULL;
	struct HAL_LLS_FW_REPORT *src;

	do {
		src = prAdapter->pucLinkStatsSrcBufferAddr;
		if (!src) {
			DBGLOG(REQ, ERROR, "EMI mapping not done");
			rStatus = -EFAULT;
			break;
		}

		buf = (uint8_t *)&prAdapter->rLinkStatsDestBuffer;
		kalMemZero(buf, sizeof(prAdapter->rLinkStatsDestBuffer));
		DBGLOG(REQ, WARN, "link state query\n");

		query.cmd.u4Tag = STATS_LLS_TAG_LLS_DATA;
		rStatus = kalIoctl(prAdapter->prGlueInfo,
				wlanQueryLinkStats,
				&query,
				u4QueryBufLen,
				&u4QueryInfoLen);

		DBGLOG(REQ, WARN, "kalIoctl=%x, %u bytes, status=%u",
					rStatus, u4QueryInfoLen,
					query.data.eUpdateStatus);

		if (rStatus != WLAN_STATUS_SUCCESS ||
			u4QueryInfoLen !=
				sizeof(struct EVENT_STATS_LLS_DATA) ||
			query.data.eUpdateStatus !=
				STATS_LLS_UPDATE_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN, "kalIoctl=%x, %u bytes, status=%u",
					rStatus, u4QueryInfoLen,
					query.data.eUpdateStatus);
			rStatus = -EFAULT;
			break;
		}

		/* Fill returning buffer */
		ptr = buf;
		ptr += p2pFuncFillIfaceCsa(ptr, src, prAdapter);

		*(uint32_t *)ptr = ENUM_BAND_NUM;
		ptr += sizeof(uint32_t);

		ptr += p2pFuncFillRadioCsa(ptr, src->radio, ENUM_BAND_NUM, prAdapter);
		DBGLOG(REQ, TRACE, "Collected %ld bytes for LLS", ptr - buf);

		p2pFuncFillCsaInfo(ptr, src, prAdapter);

		return;
	} while (0);

	return;

}
#endif

#if CFG_STAINFO_FEATURE
uint32_t wlanoidGetStaInfoAll(struct ADAPTER *prAdapter,
			void *pvSetBuffer,
			uint32_t u4SetBufferLen,
			uint32_t *pu4SetInfoLen)
{
	char separator[] = "\0";
	char cmd[] = "GETSTAINFO";
	char retryDefault[] = "Rx_Retry_Pkts=";
	char packetsDefault[] = "Rx_BcMc_Pkts=";
	char capDefault[] = "CAP=-1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1";
	char error[] = "Not in SAP";
	char error2[] = "No connection on SAP";
	struct BSS_INFO *prBssInfo;
	struct LINK *prClientList;
	struct STA_RECORD *prStaRec;
	uint16_t index = 0;
	uint8_t j = 0;
	uint32_t tempout;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct net_device *pvDevHandler = NULL;

	prBssInfo = prAdapter->prBssInfoForQuery;

	if (prBssInfo == NULL) {
		index += kalSprintf(pvSetBuffer + index, "%s\0", error);
		*pu4SetInfoLen = index;
		DBGLOG(OID, WARN, "No hotspot is found\n");
		return WLAN_STATUS_SUCCESS;
	}
	prClientList = &prBssInfo->rStaRecOfClientList;
	prStaRec = &prAdapter->rSapLastStaRec;

	if (prAdapter->fgSapLastStaRecSet == 0) {
		index += kalSprintf(pvSetBuffer + index, "%s\0", error2);
		*pu4SetInfoLen = index;
		DBGLOG(OID, WARN, "prStaRec is null\n");
		return WLAN_STATUS_SUCCESS;
	}
	/* CMD */
	index += kalSprintf(pvSetBuffer + index, "%s ", cmd);

	/* remote Device Address MAC */
	index += kalSprintf(pvSetBuffer + index, "ALL ");

	DBGLOG(OID, TRACE, "OUI =%x:%x:%x:%x:%x:%x\n",
		prStaRec->aucMacAddr[0],
		prStaRec->aucMacAddr[1],
		prStaRec->aucMacAddr[2],
		prStaRec->aucMacAddr[3],
		prStaRec->aucMacAddr[4],
		prStaRec->aucMacAddr[5]);

	/* Rx retry packets : */
	index += kalSprintf(pvSetBuffer + index, "%s", retryDefault);

	tempout = prAdapter->rQueryStaInfo.u4RxRetryPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);
	/* Rx broadcast multicast packets : */
	index += kalSprintf(pvSetBuffer + index, "%s", packetsDefault);

	tempout =prAdapter->rQueryStaInfo.u4RxBcMcPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* Capability */
	index += kalSprintf(pvSetBuffer + index, "%s ", capDefault);

	/* tx rate*/
	for (j = (MAX_STA_INFO_MCS_NUM/2);	j > 0; j--) {
		tempout = prAdapter->rQueryStaInfo.u2TxRateAll[j-1] +
			prAdapter->rQueryStaInfo.u2TxRateAll[j-1 +
				(MAX_STA_INFO_MCS_NUM/2)];
		if (j == 1)
			index += kalSprintf(pvSetBuffer + index, "%d=%d ",j-1, tempout);
		else
			index += kalSprintf(pvSetBuffer + index, "%d=%d,",j-1, tempout);
	}

	pvDevHandler =
		wlanGetNetDev(prGlueInfo, prBssInfo->ucBssIndex);
	if (!pvDevHandler) {
		DBGLOG(QM, WARN, "pvDevHandler NULL\n");
		return WLAN_STATUS_SUCCESS;
	}
	/* tx packet*/
	tempout = pvDevHandler->stats.tx_packets;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "tx packet =%d\n", tempout);

	/* tx failure*/
	tempout = prAdapter->rQueryStaInfo.u2TxFailPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "tx FailPkts =%d\n", tempout);

	/* tx retry*/
	tempout = prAdapter->rQueryStaInfo.u2TxRetryPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "tx RetryPkts =%d\n", tempout);

	for (j = (MAX_STA_INFO_MCS_NUM/2);	j > 0; j--) {
		tempout = prAdapter->rQueryStaInfo.u2RxRateAll[j-1] +
			prAdapter->rQueryStaInfo.u2RxRateAll[j-1 +
				(MAX_STA_INFO_MCS_NUM/2)];
		if (j == 1)
			index += kalSprintf(pvSetBuffer + index, "%d=%d ",j-1, tempout);
		else
			index += kalSprintf(pvSetBuffer + index, "%d=%d,",j-1, tempout);
	}

	/* rx packet*/
	tempout = pvDevHandler->stats.rx_packets;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "rx packet =%d\n", tempout);

	/* rx failure*/
	tempout = prAdapter->rQueryStaInfo.u2RxFailPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "rx FailPkts =%d\n", tempout);

	/* rx retry*/
	tempout = prAdapter->rQueryStaInfo.u2RxRetryPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "rx RetryPkts =%d\n", tempout);

	kalMemCopy(pvSetBuffer + index, separator, 1);
	index++;
	*pu4SetInfoLen = index;

	DBGLOG(OID, INFO, "index=%d\n", index);
	DBGLOG(OID, INFO, "output %s \n", pvSetBuffer);
	return WLAN_STATUS_SUCCESS;
}
#endif

#if CFG_STAINFO_FEATURE
uint32_t wlanoidGetStaInfo(struct ADAPTER *prAdapter,
			void *pvSetBuffer,
			uint32_t u4SetBufferLen,
			uint32_t *pu4SetInfoLen)
{
	char separator[] = "\0";
	char cmd[] = "GETSTAINFO";
	char retryDefault[] = "Rx_Retry_Pkts=";
	char packetsDefault[] = "Rx_BcMc_Pkts=";
	char capDefault[] = "CAP=-1";
	char error[] = "Not in SAP";
	char error2[] = "No connection on SAP";
	struct BSS_INFO *prBssInfo;
	struct LINK *prClientList;
	struct STA_RECORD *prStaRec;
	uint16_t index = 0;
	uint8_t j = 0;
	uint32_t tempout;
	uint8_t ucVhtOpModeChannelWidth = 0;
	uint8_t ucVhtOpModeRxNss = 0;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct net_device *pvDevHandler = NULL;

	prBssInfo = prAdapter->prBssInfoForQuery;

	if (prBssInfo == NULL) {
		index += kalSprintf(pvSetBuffer + index, "%s\0", error);
		*pu4SetInfoLen = index;
		DBGLOG(OID, WARN, "No hotspot is found\n");
		return WLAN_STATUS_SUCCESS;
	}
	prClientList = &prBssInfo->rStaRecOfClientList;
	prStaRec = &prAdapter->rSapLastStaRec;

	if (prAdapter->fgSapLastStaRecSet == 0) {
		index += kalSprintf(pvSetBuffer + index, "%s\0", error2);
		*pu4SetInfoLen = index;
		DBGLOG(OID, WARN, "prStaRec is null\n");
		return WLAN_STATUS_SUCCESS;
	}
	/* CMD */
	index += kalSprintf(pvSetBuffer + index, "%s ", cmd);

	/* remote Device Address MAC */
	index += kalSprintf(pvSetBuffer + index, "%x:%x:%x:%x:%x:%x ",
		prStaRec->aucMacAddr[0],
		prStaRec->aucMacAddr[1],
		prStaRec->aucMacAddr[2],
		prStaRec->aucMacAddr[3],
		prStaRec->aucMacAddr[4],
		prStaRec->aucMacAddr[5]);

	DBGLOG(OID, TRACE, "OUI =%x:%x:%x:%x:%x:%x\n",
		prStaRec->aucMacAddr[0],
		prStaRec->aucMacAddr[1],
		prStaRec->aucMacAddr[2],
		prStaRec->aucMacAddr[3],
		prStaRec->aucMacAddr[4],
		prStaRec->aucMacAddr[5]);

	/* Rx retry packets : */
	index += kalSprintf(pvSetBuffer + index, "%s", retryDefault);

	tempout = prAdapter->rQueryStaInfo.u4RxRetryPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);
	/* Rx broadcast multicast packets : */
	index += kalSprintf(pvSetBuffer + index, "%s", packetsDefault);

	tempout =prAdapter->rQueryStaInfo.u4RxBcMcPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* Capability */
	index += kalSprintf(pvSetBuffer + index, "%s ", capDefault);

	/* OUI */
	index += kalSprintf(pvSetBuffer + index, "%x:%x:%x ",
		prStaRec->aucMacAddr[0],
		prStaRec->aucMacAddr[1],
		prStaRec->aucMacAddr[2]);

	DBGLOG(OID, TRACE, "OUI =%x:%x:%x\n",
		prStaRec->aucMacAddr[0],
		prStaRec->aucMacAddr[1],
		prStaRec->aucMacAddr[2]);
	DBGLOG(OID, INFO, "output %s \n", pvSetBuffer);

	/* Channel */
	tempout = prBssInfo->ucPrimaryChannel;
	tempout = nicChannelNum2Freq(tempout, prBssInfo->eBand)/1000;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "Channel =%d\n", tempout);

	/* BW */
	ucVhtOpModeChannelWidth = (prStaRec->ucVhtOpMode &
					VHT_OP_MODE_CHANNEL_WIDTH);

	switch (ucVhtOpModeChannelWidth) {
	case VHT_OP_MODE_CHANNEL_WIDTH_20:
		tempout = 20;
		break;
	case VHT_OP_MODE_CHANNEL_WIDTH_40:
		tempout = 40;
		break;
	case VHT_OP_MODE_CHANNEL_WIDTH_80:
		tempout = 80;
		break;
	case VHT_OP_MODE_CHANNEL_WIDTH_160_80P80:
		tempout = 160;
		break;
	}
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "BW =%d\n", tempout);

	/* RSSI */
	tempout = prAdapter->rQueryStaInfo.rRssi;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "RSSI =%d\n", tempout);

	/* Data rate */

	tempout = prGlueInfo
		->u4TxLinkSpeedCache[prBssInfo->ucBssIndex]/2;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "Data rate =%d\n", tempout);

	/* 802.11 mode */
	tempout = 0;
	if (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11BG ||
		prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11ABG ||
		prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11ABGN) {
		tempout |= BIT(0);
	}
	if (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)
		tempout |= BIT(1);
	if (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AX)
		tempout |= BIT(2);
	if (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11BE)
		tempout |= BIT(4);

	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* Antenna mode */
	ucVhtOpModeRxNss = (prStaRec->ucVhtOpMode & VHT_OP_MODE_RX_NSS) >>
		VHT_OP_MODE_RX_NSS_OFFSET;
	tempout = ucVhtOpModeRxNss + 1;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* MU-MIMO : return 0 */
	tempout = 0;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* WFA reason */
	tempout = prBssInfo->u2DeauthReason;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, INFO, "WFA reason =%d\n", tempout);

	/* Supported band */
	tempout = prBssInfo->eBand - 1;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* ARSSI */
	tempout = prAdapter->rQueryStaInfo.rRssi;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "ARSSI =%d\n", tempout);

	/* tx rate*/
	for (j = (MAX_STA_INFO_MCS_NUM/2);	j > 0; j--) {
		tempout = prAdapter->rQueryStaInfo.u2TxRate[j-1] +
			prAdapter->rQueryStaInfo.u2TxRate[j-1 +
				(MAX_STA_INFO_MCS_NUM/2)];
		if (j == 1)
			index += kalSprintf(pvSetBuffer + index, "%d=%d ",j-1, tempout);
		else
			index += kalSprintf(pvSetBuffer + index, "%d=%d,",j-1, tempout);
	}

	pvDevHandler =
		wlanGetNetDev(prGlueInfo, prBssInfo->ucBssIndex);
	if (!pvDevHandler) {
		DBGLOG(QM, WARN, "pvDevHandler NULL\n");
		return WLAN_STATUS_SUCCESS;
	}
	/* tx packet*/
	tempout = pvDevHandler->stats.tx_packets;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "tx packet =%d\n", tempout);

	/* tx failure*/
	tempout = prAdapter->rQueryStaInfo.u2TxFailPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "tx FailPkts =%d\n", tempout);

	/* tx retry*/
	tempout = prAdapter->rQueryStaInfo.u2TxRetryPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "tx RetryPkts =%d\n", tempout);

	for (j = (MAX_STA_INFO_MCS_NUM/2);	j > 0; j--) {
		tempout = prAdapter->rQueryStaInfo.u2RxRate[j-1] +
			prAdapter->rQueryStaInfo.u2RxRate[j-1 +
				(MAX_STA_INFO_MCS_NUM/2)];
		if (j == 1)
			index += kalSprintf(pvSetBuffer + index, "%d=%d ",j-1, tempout);
		else
			index += kalSprintf(pvSetBuffer + index, "%d=%d,",j-1, tempout);
	}

	/* rx packet*/
	tempout = pvDevHandler->stats.rx_packets;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "rx packet =%d\n", tempout);

	/* rx failure*/
	tempout = prAdapter->rQueryStaInfo.u2RxFailPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "rx FailPkts =%d\n", tempout);

	/* rx retry*/
	tempout = prAdapter->rQueryStaInfo.u2RxRetryPkts;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "rx RetryPkts =%d\n", tempout);

	kalMemCopy(pvSetBuffer + index, separator, 1);
	index++;
	*pu4SetInfoLen = index;

	DBGLOG(OID, INFO, "index=%d\n", index);
	DBGLOG(OID, INFO, "output %s \n", pvSetBuffer);
	return WLAN_STATUS_SUCCESS;
}
#else
uint32_t wlanoidGetStaInfo(struct ADAPTER *prAdapter,
			void *pvSetBuffer,
			uint32_t u4SetBufferLen,
			uint32_t *pu4SetInfoLen)
{
	char separator[] = "\0";
	char cmd[] = "GETSTAINFO";
	char retryDefault[] = "Rx_Retry_Pkts=0";
	char packetsDefault[] = "Rx_BcMc_Pkts=0";
	char capDefault[] = "CAP=0000";
	char error[] = "Not in SAP";
	char error2[] = "No connection on SAP";
	struct BSS_INFO *prBssInfo;
	struct LINK *prClientList;
	struct STA_RECORD *prStaRec;
	uint8_t index = 0;
	uint32_t tempout;
	uint8_t ucVhtOpModeChannelWidth = 0;
	uint8_t ucVhtOpModeRxNss = 0;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;

	*pu4SetInfoLen = 0;

	prBssInfo = cnmGetSapBssInfo(prAdapter);

	if (prBssInfo == NULL) {
		index += kalSprintf(pvSetBuffer + index, "%s\0", error);
		*pu4SetInfoLen = index;
		DBGLOG(OID, WARN, "No hotspot is found\n");
		return WLAN_STATUS_SUCCESS;
	}
	prClientList = &prBssInfo->rStaRecOfClientList;
	prStaRec = &prAdapter->rSapLastStaRec;

	if (prAdapter->fgSapLastStaRecSet == 0) {
		index += kalSprintf(pvSetBuffer + index, "%s\0", error2);
		*pu4SetInfoLen = index;
		DBGLOG(OID, WARN, "prStaRec is null\n");
		return WLAN_STATUS_SUCCESS;
	}
	/* CMD */
	index += kalSprintf(pvSetBuffer + index, "%s ", cmd);

	/* remote Device Address MAC */
	index += kalSprintf(pvSetBuffer + index,
		"%02x,%02x,%02x,%02x,%02x,%02x ",
		prStaRec->aucMacAddr[0],
		prStaRec->aucMacAddr[1],
		prStaRec->aucMacAddr[2],
		prStaRec->aucMacAddr[3],
		prStaRec->aucMacAddr[4],
		prStaRec->aucMacAddr[5]);

	DBGLOG(OID, TRACE, "OUI =%02x,%02x,%02x,%02x,%02x,%02x\n",
		prStaRec->aucMacAddr[0],
		prStaRec->aucMacAddr[1],
		prStaRec->aucMacAddr[2],
		prStaRec->aucMacAddr[3],
		prStaRec->aucMacAddr[4],
		prStaRec->aucMacAddr[5]);

	/* Rx retry packets : not support */
	index += kalSprintf(pvSetBuffer + index, "%s ", retryDefault);

	/* Rx broadcast multicast packets : not support */
	index += kalSprintf(pvSetBuffer + index, "%s ", packetsDefault);

	/* Capability */
	index += kalSprintf(pvSetBuffer + index, "%s ", capDefault);

	/* OUI */
	index += kalSprintf(pvSetBuffer + index, "%02x,%02x,%02x ",
		prStaRec->aucMacAddr[0],
		prStaRec->aucMacAddr[1],
		prStaRec->aucMacAddr[2]);

	DBGLOG(OID, TRACE, "OUI =%x,%x,%x\n",
		prStaRec->aucMacAddr[0],
		prStaRec->aucMacAddr[1],
		prStaRec->aucMacAddr[2]);

	/* Channel */
	tempout = prBssInfo->ucPrimaryChannel;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "Channel =%d\n", tempout);

	/* BW */
	ucVhtOpModeChannelWidth = (prStaRec->ucVhtOpMode &
					VHT_OP_MODE_CHANNEL_WIDTH);

	switch (ucVhtOpModeChannelWidth) {
	case VHT_OP_MODE_CHANNEL_WIDTH_20:
		tempout = 20;
		break;
	case VHT_OP_MODE_CHANNEL_WIDTH_40:
		tempout = 40;
		break;
	case VHT_OP_MODE_CHANNEL_WIDTH_80:
		tempout = 80;
		break;
	case VHT_OP_MODE_CHANNEL_WIDTH_160_80P80:
		tempout = 160;
		break;
	}
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "BW =%d\n", tempout);

	/* RSSI */
	tempout = prGlueInfo->i4RssiCache[prBssInfo->ucBssIndex];
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "RSSI =%d\n", tempout);

	/* Data rate */
	tempout = prGlueInfo->u4TxLinkSpeedCache[prBssInfo->ucBssIndex];
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "Data rate =%d\n", tempout);

	/* 802.11 mode */
	tempout = 0;
	if (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11BG ||
		prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11ABG ||
		prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11ABGN) {
		tempout |= BIT(0);
	}
	if (prStaRec->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)
		tempout |= BIT(1);
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* Antenna mode */
	ucVhtOpModeRxNss = (prStaRec->ucVhtOpMode & VHT_OP_MODE_RX_NSS) >>
		VHT_OP_MODE_RX_NSS_OFFSET;
	tempout = ucVhtOpModeRxNss + 1;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* MU-MIMO : return 0 */
	tempout = 0;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* WFA reason */
	tempout = prBssInfo->u2DeauthReason;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, INFO, "WFA reason =%d\n", tempout);

	/* Supported band */
	if (prStaRec->ucSupportedBand != 0)
		tempout = prStaRec->ucSupportedBand;
	else
		tempout = prBssInfo->eBand - 1;
	index += kalSprintf(pvSetBuffer + index, "%d", tempout);

	kalMemCopy(pvSetBuffer + index, separator, 1);
	index++;
	*pu4SetInfoLen = index;

	DBGLOG(OID, INFO, "index=%d\n", index);
	return WLAN_STATUS_SUCCESS;
}
#endif

#if CFG_STAINFO_FEATURE

int p2pFuncGetSingleStaTrxInfo(
	struct GLUE_INFO *prGlueInfo,
	struct ADAPTER *prAdapter,
	struct STA_RECORD *prStaRec,
	struct BSS_INFO *prBssInfo,
	uint8_t *aucMacAddr,
	void *pvSetBuffer,
	uint32_t *pu4AllInfoLen,
	u_int8_t fgLoopAllSta)
{
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics = NULL;
	struct CHIP_DBG_OPS *prChipDbg = NULL;
	u_int32_t u4BufLen = 0;
#if 0
	struct PARAM_HW_WLAN_INFO *prHwWlanInfo;
	uint8_t txmode, rate, stbc, sgi, nsts;
	uint32_t u4CurRate;
#endif
	int32_t i4Rssi0 = 0;
	int32_t i4Rssi1 = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4BytesWritten = 0;

	union {
		struct CMD_GET_STATS_LLS cmd;
		struct EVENT_STATS_LLS_DATA data;
	} query = {0};

	uint32_t u4QueryBufLen = sizeof(query);
	uint32_t u4QueryInfoLen = sizeof(query.cmd);
	uint8_t *buf = NULL;
	uint8_t *ptr = NULL;
	struct HAL_LLS_FW_REPORT *src;
	uint32_t u4Len = 0;
	uint8_t rsp[500];
	uint32_t u4SetInfoLen = 0;

	/* Get Statistics info */
	prQueryStaStatistics = (struct PARAM_GET_STA_STATISTICS *)
		kalMemAlloc(sizeof(struct PARAM_GET_STA_STATISTICS),
		VIR_MEM_TYPE);
	if (!prQueryStaStatistics) {
		DBGLOG(REQ, ERROR,
			"Allocate memory for prQueryStaStatistics failed!\n");
		i4BytesWritten = -1;
		goto exit;
	}

	COPY_MAC_ADDR(prQueryStaStatistics->aucMacAddr, prStaRec->aucMacAddr);
	prQueryStaStatistics->ucReadClear = TRUE;

	DBGLOG(REQ, ERROR,
		"Query "MACSTR"\n", MAC2STR(prQueryStaStatistics->aucMacAddr));
	rStatus = kalIoctl(prGlueInfo,
				wlanoidQueryStaStatistics, prQueryStaStatistics,
				sizeof(struct PARAM_GET_STA_STATISTICS),
				&u4BufLen);
	prGlueInfo->u4TxLinkSpeedCache[prBssInfo->ucBssIndex]
			= prQueryStaStatistics->u2LinkSpeed;
	DBGLOG(REQ, ERROR, "link speed: %u",
		prQueryStaStatistics->u2LinkSpeed);

	prAdapter->rQueryStaInfo.u4RxRetryPkts = prStaRec->u4RxRetryCnt;
	prAdapter->rQueryStaInfo.u4RxBcMcPkts = prStaRec->u4RxBmcCnt;
	DBGLOG(REQ, ERROR, "u4RxRetryPkts: %u",
		prAdapter->rQueryStaInfo.u4RxRetryPkts);
	DBGLOG(REQ, ERROR, "u4RxBcMcPktsa: %u",
		prAdapter->rQueryStaInfo.u4RxBcMcPkts);

	prChipDbg = prGlueInfo->prAdapter->chip_info->prDebugOps;
	if (prChipDbg && prChipDbg->get_rssi_from_wtbl) {
		prChipDbg->get_rssi_from_wtbl(prAdapter,
			prStaRec->ucWlanIndex, &i4Rssi0, &i4Rssi1,
			NULL, NULL);
		DBGLOG(REQ, ERROR, "i4Rssi0: %d and i4Rssi1: %d",
			i4Rssi0, i4Rssi1);
	}

	prAdapter->rQueryStaInfo.rRssi = (i4Rssi0 + i4Rssi1)/2;

	do {
		prAdapter = prGlueInfo->prAdapter;
		if(!prAdapter)
			break;

		src = prAdapter->pucLinkStatsSrcBufferAddr;
		if (!src) {
			DBGLOG(REQ, ERROR, "EMI mapping not done");
			rStatus = -EFAULT;
			break;
		}

#if (CFG_EXT_VERSION >= 2)
		buf = (uint8_t *)&prAdapter->rLinkStatsDestBuffer;
		kalMemZero(buf, sizeof(prAdapter->rLinkStatsDestBuffer));
#else
		buf = (uint8_t *)&prAdapter->rSapLinkStatsDestBuffer;
		kalMemZero(buf, sizeof(prAdapter->rSapLinkStatsDestBuffer));
#endif
		query.cmd.u4Tag = STATS_LLS_TAG_LLS_DATA;
		rStatus = kalIoctl(prGlueInfo,
			   wlanQueryLinkStats,
			   &query, /* pvInfoBuf */
			   u4QueryBufLen, /* u4InfoBufLen */
			   &u4QueryInfoLen); /* pu4QryInfoLen */

		DBGLOG(REQ, WARN, "kalIoctl=%x, %u bytes, status=%u",
					rStatus, u4QueryInfoLen,
					query.data.eUpdateStatus);


		if (rStatus != WLAN_STATUS_SUCCESS ||
			u4QueryInfoLen !=
				sizeof(struct EVENT_STATS_LLS_DATA) ||
			query.data.eUpdateStatus !=
				STATS_LLS_UPDATE_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN, "kalIoctl=%x, %u bytes, status=%u",
					rStatus, u4QueryInfoLen,
					query.data.eUpdateStatus);
			rStatus = -EFAULT;
			break;
		}

		/* Fill returning buffer */

		ptr = buf;
		ptr += p2pFuncFillIfaceCsa(ptr, src, prAdapter);

		*(uint32_t *)ptr = ENUM_BAND_NUM;
		ptr += sizeof(uint32_t);
	} while (0);
	kalMemCopy(&prGlueInfo->prAdapter->rSapLastStaRec,
		prStaRec, sizeof(struct STA_RECORD));

	prGlueInfo->prAdapter->fgSapLastStaRecSet = 1;
	kalMemCopy(&prGlueInfo->prAdapter->rSapLastStaRec,
		prStaRec, sizeof(struct STA_RECORD));
	prAdapter->prBssInfoForQuery = prBssInfo;

	kalMemZero(rsp, sizeof(rsp));
	if (fgLoopAllSta == TRUE) {
		uint8_t j = 0;

		for (j = 0;	j < MAX_STA_INFO_MCS_NUM; j++) {
			prAdapter->rQueryStaInfo.u2TxRateAll[j] =
				prAdapter->rQueryStaInfo.u2TxRateAll[j] +
				prAdapter->rQueryStaInfo.u2TxRate[j];
			prAdapter->rQueryStaInfo.u2RxRateAll[j] =
				prAdapter->rQueryStaInfo.u2RxRateAll[j] +
				prAdapter->rQueryStaInfo.u2RxRate[j];
		}
		rStatus = kalIoctl(prGlueInfo,
			wlanoidGetStaInfoAll,
			(void *)rsp, u4Len, &u4SetInfoLen);

	} else
		rStatus = kalIoctl(prGlueInfo,
			wlanoidGetStaInfo,
			(void *)rsp, u4Len, &u4SetInfoLen);

	kalMemCopy(pvSetBuffer, rsp, u4SetInfoLen);
	*pu4AllInfoLen = u4SetInfoLen;

	return rStatus;

	exit:

		if (prQueryStaStatistics)
			kalMemFree(prQueryStaStatistics,
				VIR_MEM_TYPE,
				sizeof(PARAM_GET_STA_STATISTICS));
		return i4BytesWritten;

}
int priv_driver_get_sta_trx_info(struct net_device *prNetDev,
				char *pcCommand, int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int32_t i4Ret = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int8_t *this_char = NULL;
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t ucRoleIdx = 0;
	uint8_t ucBssIdx = 0;
	struct BSS_INFO *prBssInfo = NULL;
	struct STA_RECORD *prStaRec = NULL;
	u_int8_t fgLoopAllSta = FALSE;
	uint8_t *rsp;
	uint32_t u4SetInfoLen = 0;
	struct wiphy *wiphy;
#if CFG_SUPPORT_LLS && CFG_REPORT_TX_RATE_FROM_LLS
	uint32_t rStatus;
	uint32_t u4TxBw, u4TxRate;
#endif

	wiphy = wlanGetWiphy();

	rsp =  (uint8_t *)
		kalMemAlloc(500, VIR_MEM_TYPE);
	if (!rsp){
		i4BytesWritten = -1;
		goto exit;
	}

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE) {
		i4BytesWritten = -1;
		goto exit;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (!prGlueInfo || prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -1;
	}

	prAdapter = prGlueInfo->prAdapter;

	if (!prAdapter) {
		DBGLOG(REQ, WARN, "Adapter is not ready\n");
		return -1;
	}

	/* get command */
	DBGLOG(REQ, ERROR, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, ERROR,
		"argc is %d, apcArgv[1] = %s\n\n", i4Argc, apcArgv[1]);

	/* get param */
	this_char = apcArgv[1];
	DBGLOG(REQ, LOUD, "string = %s\n", this_char);
	if (kalStrnCmp(this_char, "ALL", 3) == 0)
		fgLoopAllSta = TRUE;

	i4Ret = wlanHwAddrToBin(this_char, &aucMacAddr[0]);
	if (i4Ret < 0 && fgLoopAllSta == FALSE) {
		DBGLOG(INIT, ERROR,
				"incorrect mac\n");
		return -1;
	}

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	DBGLOG(REQ, INFO, "ucRoleIdx = %d\n", ucRoleIdx);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
	if (!prBssInfo) {
		DBGLOG(REQ, WARN, "bss is not active\n");
		i4BytesWritten = -1;
		goto exit;
	}
#if CFG_SUPPORT_LLS && CFG_REPORT_TX_RATE_FROM_LLS
	rStatus = wlanGetTxRateFromLinkStats(prGlueInfo, &u4TxRate,
			&u4TxBw, prBssInfo->ucBssIndex);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		prGlueInfo
			->u4TxLinkSpeedCache[prBssInfo->ucBssIndex]
			= u4TxRate;
		prGlueInfo->u4TxBwCache[prBssInfo->ucBssIndex]
			= u4TxBw;
	}
#endif

	if (fgLoopAllSta == TRUE) {
		struct LINK *prClientList;

		prClientList = &prBssInfo->rStaRecOfClientList;
		if (prClientList->u4NumElem == 0) {
			DBGLOG(REQ, WARN, "can't find station\n");
			i4BytesWritten = -1;
			goto exit;
		}

		kalMemZero(prAdapter->rQueryStaInfo.u2TxRateAll,
			sizeof(prAdapter->rQueryStaInfo.u2TxRateAll));
		kalMemZero(prAdapter->rQueryStaInfo.u2RxRateAll,
			sizeof(prAdapter->rQueryStaInfo.u2RxRateAll));

		LINK_FOR_EACH_ENTRY(prStaRec,
		prClientList, rLinkEntry, struct STA_RECORD) {

			i4BytesWritten =
				p2pFuncGetSingleStaTrxInfo(
					prGlueInfo, prAdapter, prStaRec,
					prBssInfo, aucMacAddr,
					rsp, &u4SetInfoLen,
					fgLoopAllSta);
		}
		goto exit;

	}
	/* get Station Record */
	prStaRec = bssGetClientByMac(prGlueInfo->prAdapter,
		prBssInfo, aucMacAddr);
	if (prStaRec == NULL) {
		DBGLOG(REQ, WARN, "can't find station\n");
		i4BytesWritten = -1;
		goto exit;
	}

	i4BytesWritten = p2pFuncGetSingleStaTrxInfo(
		prGlueInfo, prAdapter, prStaRec,
		prBssInfo, aucMacAddr,
		rsp, &u4SetInfoLen,
		fgLoopAllSta);

exit:

	if (i4BytesWritten == WLAN_STATUS_SUCCESS)
		i4BytesWritten =
			mtk_cfg80211_process_str_cmd_reply(wiphy,
				rsp, u4SetInfoLen + 1);
	if (rsp)
		kalMemFree(rsp, VIR_MEM_TYPE, 4800);
	return i4BytesWritten;
}

uint32_t wlanoidGetBssInfo(struct ADAPTER *prAdapter,
			void *pvSetBuffer,
			uint32_t u4SetBufferLen,
			uint32_t *pu4SetInfoLen)
{
	char separator[] = "\0";
	struct BSS_INFO *prBssInfo;
	uint8_t index = 0;
	uint32_t tempout;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct BSS_DESC *prBssDesc;
	struct STA_RECORD *prStaRec;
	struct AIS_FSM_INFO *prAisFsmInfo;
	/* STA only */
	uint8_t ucBssIndex = 0;
	u_int32_t ucConnectedBandwidth = 0;

#ifndef AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION
#define AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION 	2
#endif
#ifndef AUTH_TYPE_FAST_BSS_TRANSITION
#define AUTH_TYPE_FAST_BSS_TRANSITION \
	BIT(AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION)
#endif

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	prBssDesc = aisGetTargetBssDesc(prAdapter, ucBssIndex);
	prStaRec = aisGetStaRecOfAP(prAdapter, ucBssIndex);
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	if (!prBssInfo || !prBssDesc || !prStaRec || !prAisFsmInfo) {
		DBGLOG(OID, WARN, "status error: %p,%p,%p,%p",
			prBssInfo, prBssDesc, prStaRec, prAisFsmInfo);
		return WLAN_STATUS_SUCCESS;
	}
	/* OUI */
	index += kalSprintf(pvSetBuffer + index, "%02x:%02x:%02x ",
		prBssInfo->aucBSSID[0],
		prBssInfo->aucBSSID[1],
		prBssInfo->aucBSSID[2]);

	DBGLOG(OID, TRACE, "OUI =%02x:%02x:%02x\n",
		prBssInfo->aucBSSID[0],
		prBssInfo->aucBSSID[1],
		prBssInfo->aucBSSID[2]);

#if 0
	/* SSID */
	kalMemCopy(pvSetBuffer + index,
		&prBssInfo->aucSSID[0], prBssInfo->ucSSIDLen);
	index += prBssInfo->ucSSIDLen;
	kalMemCopy(pvSetBuffer + index, separator, 1);
	index++;

	DBGLOG(OID, TRACE, "SSID =%s, len=%d\n",
		prBssInfo->aucSSID, prBssInfo->ucSSIDLen);

	if (prBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED) {
		/* return 0 for following attribute */
		kalMemZero(pvSetBuffer + index, 14 * sizeof(uint32_t));
		*pu4SetInfoLen = index + 14 * sizeof(uint32_t);
		DBGLOG(OID, INFO, "no connection to AP");
		return WLAN_STATUS_SUCCESS;
	}
#endif

	/* Channel ex 2412 */
	tempout = nicChannelNum2Freq(prBssInfo->ucPrimaryChannel,
				prBssInfo->eBand) / 1000;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "Channel =%d\n", tempout);

	/* BW */
	if (IS_BSS_AIS(prBssInfo)) {
		if (prBssInfo->eBssSCO == CHNL_EXT_SCN) {
			ucConnectedBandwidth = 20;
		} else if (prBssInfo->eBssSCO != CHNL_EXT_SCN) {
			switch (prBssInfo->ucVhtChannelWidth) {
			case CW_20_40MHZ:
				ucConnectedBandwidth = 40;
				break;
			case CW_80MHZ:
				ucConnectedBandwidth = 80;
				break;
			case CW_160MHZ:
				ucConnectedBandwidth = 160;
				break;
			case CW_80P80MHZ:
				ucConnectedBandwidth = 160;
				break;
			case CW_320_1MHZ:
				ucConnectedBandwidth = 320;
				break;
			case CW_320_2MHZ:
				ucConnectedBandwidth = 320;
				break;
			}
		}
		DBGLOG(RLM, INFO, "AIS connected bandwidth=%d\n",
				ucConnectedBandwidth);
	}

	tempout = ucConnectedBandwidth;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "BW =%d\n", tempout);

	/* RSSI */
	tempout = prGlueInfo->i4RssiCache[ucBssIndex];
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "RSSI =%d\n", tempout);

	/* Data rate */
	tempout = prGlueInfo->u4TxLinkSpeedCache[ucBssIndex];
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	DBGLOG(OID, TRACE, "Data rate =%d\n", tempout);

	/* 802.11 mode */
	tempout = 0;
	if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11BE)
		tempout = 6;
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11AX)
		tempout = 5;
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)
		tempout = 4;
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11A)
		tempout = 3;
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11N)
		tempout = 2;
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11G)
		tempout = 1;
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11B)
		tempout = 0;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* Antenna mode */
	tempout = prBssInfo->ucOpTxNss - 1;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* MU-MIMO : return 0 */
	tempout = 0;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* Passpoint : return 0 */
	tempout = 0;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* SNR : not support return 0 */
	tempout = 0;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* Nosie : not support return 0 */
	tempout = 0;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* AKM */
	if (prBssInfo->u4RsnSelectedAKMSuite == RSN_AKM_SUITE_FT_OVER_SAE)
		tempout = 5;
	else if (prBssDesc->ucIsAdaptive11r)
		tempout = 4;
	else if (prAisFsmInfo->ucAvailableAuthTypes ==
			AUTH_TYPE_FAST_BSS_TRANSITION)
		tempout = 2;
	else if (rsnSearchPmkidEntry(prAdapter,
			prBssDesc->aucBSSID, ucBssIndex))
		tempout = 1;
	else
		tempout = 0;

	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* Roaming */
	tempout = prAisFsmInfo->u2ConnectedCount;
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* 11KV static */
	tempout = 0;
	/* 11K */
	if (prBssDesc->aucRrmCap[0] &
		    BIT(RRM_CAP_INFO_NEIGHBOR_REPORT_BIT))
		tempout |= BIT(0);
	/* 11V */
#if (CFG_EXT_VERSION == 1)
	if (prStaRec->fgSupportBTM)
#else
	if (prBssDesc->fgSupportBTM)
#endif
		tempout |= BIT(1);
	index += kalSprintf(pvSetBuffer + index, "%d ", tempout);

	/* KEIE supported */
	tempout = 0;
	if (*(uint32_t *)prBssDesc->aucRrmCap &
		    BIT(RRM_CAP_INFO_QBSS_LOAD_BIT))
		tempout |= BIT(0);
	if (prStaRec->fgSupportProxyARP)
		tempout |= BIT(1);
	if (prStaRec->fgSupportTFS)
		tempout |= BIT(2);
	if (prStaRec->fgSupportWNMSleep)
		tempout |= BIT(3);
	if (prStaRec->fgSupportTIMBcast)
		tempout |= BIT(4);
#if (CFG_EXT_VERSION == 1)
	if (prStaRec->fgSupportBTM)
#else
	if (prBssDesc->fgSupportBTM)
#endif
		tempout |= BIT(5);
	if (prStaRec->fgSupportDMS)
		tempout |= BIT(6);
	if (*(uint32_t *)prBssDesc->aucRrmCap &
		    BIT(RRM_CAP_INFO_LINK_MEASURE_BIT))
		tempout |= BIT(7);
	if (*(uint32_t *)prBssDesc->aucRrmCap &
		    BIT(RRM_CAP_INFO_NEIGHBOR_REPORT_BIT))
		tempout |= BIT(8);
	if (*(uint32_t *)prBssDesc->aucRrmCap &
		    BIT(RRM_CAP_INFO_BEACON_PASSIVE_MEASURE_BIT))
		tempout |= BIT(9);
	if (*(uint32_t *)prBssDesc->aucRrmCap &
		    BIT(RRM_CAP_INFO_BEACON_ACTIVE_MEASURE_BIT))
		tempout |= BIT(10);
	if (*(uint32_t *)prBssDesc->aucRrmCap &
		    BIT(RRM_CAP_INFO_BEACON_TABLE_BIT))
		tempout |= BIT(11);
	if (*(uint32_t *)prBssDesc->aucRrmCap &
		    BIT(RRM_CAP_INFO_BSS_AVG_DELAY_BIT))
		tempout |= BIT(12);

	index += kalSprintf(pvSetBuffer + index, "%d", tempout);

	kalMemCopy(pvSetBuffer + index, separator, 1);
	index++;
	*pu4SetInfoLen = index;

	DBGLOG(OID, INFO, "index=%d\n", index);
	return WLAN_STATUS_SUCCESS;
}

int testmode_get_bssinfo(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4SetInfoLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Len = 0;
	uint8_t rsp[200];

	WIPHY_PRIV(wiphy, prGlueInfo);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc > 2) {
		DBGLOG(REQ, ERROR,
			"Error input parameters(%d):%s\n",
			i4Argc,
			pcCommand);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalMemZero(rsp, sizeof(rsp));
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidGetBssInfo,
			   (void *)rsp, u4Len,
			   &u4SetInfoLen);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
					rsp, u4SetInfoLen + 1);
}

int testmode_get_sta_info(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
#if CFG_STAINFO_FEATURE
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Len = 0;
	uint8_t rsp[500];
	uint8_t aucMacAddr[MAC_ADDR_LEN] = {0};
	int32_t i4Argc = 0, i4Ret = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	struct PARAM_GET_STA_STATISTICS rQueryStaStatistics;
	u_int32_t u4BufLen = 0;
	struct HAL_LLS_FW_REPORT *src;
#if CFG_STAINFO_FEATURE
	struct CHIP_DBG_OPS *prChipDbg = NULL;
	int32_t i4Rssi0, i4Rssi1, i4RssiR;
#endif
	uint32_t u4SetInfoLen = 0;
	uint8_t ucWlanIndex;
	uint8_t ucBssIndex = 0;

	union {
		struct CMD_GET_STATS_LLS cmd;
		struct EVENT_STATS_LLS_DATA data;
	} query = {0};
	uint32_t u4QueryBufLen = sizeof(query);
	uint32_t u4QueryInfoLen = sizeof(query.cmd);
	uint8_t *buf = NULL;
	uint8_t *ptr = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	WIPHY_PRIV(wiphy, prGlueInfo);


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (!prGlueInfo || i4Argc < 3)
		return -EOPNOTSUPP;

	if (i4Argc < 2 ||
		kalStrLen(apcArgv[1]) != 17)
		return -EOPNOTSUPP;
	i4Ret = mtk_cfg80211_inspect_mac_addr(apcArgv[1]);
	if (i4Ret) {
	DBGLOG(REQ, ERROR,
		"inspect mac format error u4Ret=%d\n", i4Ret);
		return -EOPNOTSUPP;
	}
	i4Ret = sscanf(apcArgv[1], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		&aucMacAddr[0], &aucMacAddr[1], &aucMacAddr[2],
		&aucMacAddr[3], &aucMacAddr[4], &aucMacAddr[5]);
	if (i4Ret != MAC_ADDR_LEN) {
		DBGLOG(REQ, ERROR, "sscanf mac format fail u4Ret=%d\n",
			i4Ret);
		return -EOPNOTSUPP;
	}
	prBssInfo = prGlueInfo->prAdapter->aprBssInfo[ucBssIndex];
	if (prBssInfo == NULL) {
		DBGLOG(REQ, WARN, "No hotspot is found\n");
		return -EOPNOTSUPP;
	}
	prStaRec = bssGetClientByMac(prGlueInfo->prAdapter,
		prBssInfo,
		aucMacAddr);

	if (prStaRec == NULL) {
		prGlueInfo->prAdapter->fgSapLastStaRecSet = 0;
		DBGLOG(REQ, WARN, "can't find station\n");
		return -EOPNOTSUPP;
	} else {
		prGlueInfo->prAdapter->fgSapLastStaRecSet = 1;
		kalMemCopy(&prGlueInfo->prAdapter->rSapLastStaRec,
			prStaRec, sizeof(struct STA_RECORD));
	}

	/* Get Statistics info */
	COPY_MAC_ADDR(&rQueryStaStatistics.aucMacAddr, aucMacAddr);
	DBGLOG(REQ, INFO,
		"Query "MACSTR"\n", MAC2STR(rQueryStaStatistics.aucMacAddr));

	kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));
	rQueryStaStatistics.ucReadClear = TRUE;

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryStaStatistics,
			   &rQueryStaStatistics,
			   sizeof(rQueryStaStatistics), &u4BufLen);
	if(!prGlueInfo->prAdapter)
		return -EOPNOTSUPP;

#if CFG_STAINFO_FEATURE
	prGlueInfo->prAdapter->rQueryStaInfo.u4RxRetryPkts =
		prStaRec->u4RxRetryCnt;
	prGlueInfo->prAdapter->rQueryStaInfo.u4RxBcMcPkts =
		prStaRec->u4RxBmcCnt;
	DBGLOG(REQ, ERROR, "u4RxRetryPkts: %u",
		prGlueInfo->prAdapter->rQueryStaInfo.u4RxRetryPkts);
	DBGLOG(REQ, ERROR, "u4RxBcMcPktsa: %u",
		prGlueInfo->prAdapter->rQueryStaInfo.u4RxBcMcPkts);
#endif

	if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter,
		&aucMacAddr[0], &ucWlanIndex))
		return -EOPNOTSUPP;

#if CFG_STAINFO_FEATURE
			prChipDbg = prGlueInfo->prAdapter->chip_info->prDebugOps;
			if (prChipDbg && prChipDbg->get_rssi_from_wtbl) {
				prChipDbg->get_rssi_from_wtbl(prGlueInfo->prAdapter,
				ucWlanIndex, &i4Rssi0, &i4Rssi1,
				&i4RssiR, &i4RssiR);
				DBGLOG(REQ, ERROR, "i4Rssi0: %d and i4Rssi0: %d",
						i4Rssi0, i4Rssi1);
			}
#endif

			do {
				src = prGlueInfo->prAdapter->pucLinkStatsSrcBufferAddr;
				if (!src) {
					DBGLOG(REQ, ERROR, "EMI mapping not done");
					rStatus = -EFAULT;
					break;
				}

				buf = (uint8_t *)&prGlueInfo->prAdapter->rLinkStatsDestBuffer;
				kalMemZero(buf,
					sizeof(prGlueInfo->prAdapter->rLinkStatsDestBuffer));

				query.cmd.u4Tag = STATS_LLS_TAG_LLS_DATA;
				rStatus = kalIoctl(prGlueInfo,
					   wlanQueryLinkStats,
					   &query, /* pvInfoBuf */
					   u4QueryBufLen, /* u4InfoBufLen */
					   &u4QueryInfoLen); /* pu4QryInfoLen */

				DBGLOG(REQ, WARN, "kalIoctl=%x, %u bytes, status=%u",
							rStatus, u4QueryInfoLen,
							query.data.eUpdateStatus);

				if (rStatus != WLAN_STATUS_SUCCESS ||
					u4QueryInfoLen !=
						sizeof(struct EVENT_STATS_LLS_DATA) ||
					query.data.eUpdateStatus !=
						STATS_LLS_UPDATE_STATUS_SUCCESS) {
					DBGLOG(REQ, WARN, "kalIoctl=%x, %u bytes, status=%u",
							rStatus, u4QueryInfoLen,
							query.data.eUpdateStatus);
					rStatus = -EFAULT;
					break;
				}

					/* Fill returning buffer */

				ptr = buf;
				ptr += p2pFuncFillIfaceCsa(ptr, src, prGlueInfo->prAdapter);

				*(uint32_t *)ptr = ENUM_BAND_NUM;
				ptr += sizeof(uint32_t);
				ptr += p2pFuncFillRadioCsa(ptr, src->radio,
					ENUM_BAND_NUM, prGlueInfo->prAdapter);

				DBGLOG(REQ, TRACE, "Collected %u bytes for LLS", ptr - buf);

				return rStatus;
			} while (0);

			kalMemZero(rsp, sizeof(rsp));
			rStatus = kalIoctl(prGlueInfo,
				wlanoidGetStaInfo,
				(void *)rsp, u4Len, &u4SetInfoLen);
			return mtk_cfg80211_process_str_cmd_reply(wiphy,
				rsp, u4SetInfoLen + 1);
#else
	DBGLOG(REQ, WARN, "not support GETSTAINFO\n");
	return -EOPNOTSUPP;
#endif
}

int testmode_get_conn_reject_info(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	uint8_t rsp[200];

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);

	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_WLAN_ON |
					WLAN_DRV_READY_CHECK_RESET)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return WLAN_STATUS_FAILURE;
	}

	prAdapter = prGlueInfo->prAdapter;

	i4BytesWritten = kalSprintf(
		rsp, "assoc_reject.status %d",
		prAdapter->u2ConnRejectStatus);

	DBGLOG(REQ, INFO,
		"command result is %d:%s\n", i4BytesWritten, rsp);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
					rsp, i4BytesWritten + 1);
}
#endif

uint32_t wlanoidGetStaDump(struct ADAPTER *prAdapter,
			void *pvSetBuffer,
			uint32_t u4SetBufferLen,
			uint32_t *pu4SetInfoLen)
{
	struct BSS_INFO *prBssInfo;
	uint8_t index = 0;
	char tmpbuf[5];
	/* STA only */
	uint8_t ucBssIndex;
#if (CFG_SUPPORT_STATS_ONE_CMD == 1)
	uint32_t u4TxRate;

#if CFG_SUPPORT_LLS
	struct _STATS_LLS_TX_RATE_INFO *prLlsRateInfo;
	uint32_t u4TxNss;
#endif

#endif
	struct RxRateInfo rRxRateInfo = {0};
	uint32_t u4CurRxRate, u4MaxRxRate;
	int rxRateStatus = -1;

	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	if (unlikely(ucBssIndex >= BSSID_NUM))
		return WLAN_STATUS_INVALID_DATA;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	/* bitrate */
	rxRateStatus = wlanGetRxRateByBssid(prAdapter->prGlueInfo, ucBssIndex,
			&u4CurRxRate, &u4MaxRxRate, &rRxRateInfo);
	if (rxRateStatus == 0) {
		index += kalSprintf(pvSetBuffer + index, " rxbitrate:%u.%uMbps",
			u4CurRxRate / 10,
			u4CurRxRate % 10);
		DBGLOG(OID, TRACE, "rxbitrate:%u.%uMbps\n",
			u4CurRxRate / 10,
			u4CurRxRate % 10);
	}

#if (CFG_SUPPORT_STATS_ONE_CMD == 1)
	u4TxRate = prAdapter->prGlueInfo->u4TxLinkSpeedCache[ucBssIndex];

	index += kalSprintf(pvSetBuffer + index, " txbitrate:%u.%uMbps",
		u4TxRate / 10, u4TxRate % 10);
	DBGLOG(OID, TRACE, "txbitrate:%u.%uMbps\n",
		u4TxRate / 10, u4TxRate % 10);
#endif

	/* MCS */
	if (rxRateStatus == 0 &&
		rRxRateInfo.u4Mode != TX_RATE_MODE_CCK &&
		rRxRateInfo.u4Mode != TX_RATE_MODE_OFDM) {
		index += kalSprintf(pvSetBuffer + index, " rxmcs:%u",
			rRxRateInfo.u4Rate);
		DBGLOG(OID, TRACE, "rxmcs:%u\n", rRxRateInfo.u4Rate);
	}

#if (CFG_SUPPORT_STATS_ONE_CMD == 1 && CFG_SUPPORT_LLS)
	prLlsRateInfo = &(
		prAdapter->prStatsAllRegStat->rLlsRateInfo.arTxRateInfo[
		ucBssIndex]);
	if (prLlsRateInfo->mode != TX_RATE_MODE_CCK &&
		prLlsRateInfo->mode != TX_RATE_MODE_OFDM) {
		index += kalSprintf(pvSetBuffer + index, " txmcs:%u",
			prLlsRateInfo->rate);
		DBGLOG(OID, TRACE, "txmcs:%u\n", prLlsRateInfo->rate);
	}
#endif

	/* BW */
	if (rxRateStatus == 0) {
		kalMemZero(tmpbuf, sizeof(tmpbuf));
		kalSnprintf(tmpbuf, 3, HW_TX_RATE_BW[rRxRateInfo.u4Bw] + 2);
		index += kalSprintf(pvSetBuffer + index, " rxbandwidth:%sMHz",
			tmpbuf);
		DBGLOG(OID, TRACE, "rxbandwidth:%sMHz\n", tmpbuf);
	}

#if (CFG_SUPPORT_STATS_ONE_CMD == 1 && CFG_SUPPORT_LLS)
	kalMemZero(tmpbuf, sizeof(tmpbuf));
	kalSnprintf(tmpbuf, 3, HW_TX_RATE_BW[prLlsRateInfo->bw] + 2);
	index += kalSprintf(pvSetBuffer + index, " txbandwidth:%sMHz",
		tmpbuf);
	DBGLOG(OID, TRACE, "txbandwidth:%sMHz\n", tmpbuf);
#endif

	/* nss */
	if (rxRateStatus == 0) {
		index += kalSprintf(pvSetBuffer + index, " rxnss:%u",
			rRxRateInfo.u4Nss);
		DBGLOG(OID, TRACE, "rxnss:%u\n", rRxRateInfo.u4Nss);
	}

#if (CFG_SUPPORT_STATS_ONE_CMD == 1 && CFG_SUPPORT_LLS)
	if (prLlsRateInfo->nsts == 1)
		u4TxNss = prLlsRateInfo->nsts;
	else
		u4TxNss = prLlsRateInfo->stbc ?
			(prLlsRateInfo->nsts >> 1)
			: prLlsRateInfo->nsts;

	index += kalSprintf(pvSetBuffer + index, " txnss:%u",
		u4TxNss + 1);
	DBGLOG(OID, TRACE, "txnss:%u\n", u4TxNss + 1);
#endif

	/* 802.11 mode */
	if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11BE)
		index += kalSprintf(pvSetBuffer + index, " mode:%s", "11be");
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11AX)
		index += kalSprintf(pvSetBuffer + index, " mode:%s", "11ax");
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11AC)
		index += kalSprintf(pvSetBuffer + index, " mode:%s", "11ac");
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11A)
		index += kalSprintf(pvSetBuffer + index, " mode:%s", "11a");
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11N)
		index += kalSprintf(pvSetBuffer + index, " mode:%s", "11n");
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11G)
		index += kalSprintf(pvSetBuffer + index, " mode:%s", "11g");
	else if (prBssInfo->ucPhyTypeSet & PHY_TYPE_SET_802_11B)
		index += kalSprintf(pvSetBuffer + index, " mode:%s", "11b");

	kalMemCopy(pvSetBuffer + index, "\0", 1);
	index++;
	*pu4SetInfoLen = index;

	DBGLOG(OID, INFO, "index=%d\n", index);
	return WLAN_STATUS_SUCCESS;
}

int priv_driver_get_sta_dump(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct net_device_stats *prDevStats = NULL;
	uint32_t index = 0;
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIdx = 0;
	char rsp[512] = {0};
#if (CFG_SUPPORT_STATS_ONE_CMD == 1)
	struct PARAM_GET_STATS_ONE_CMD rParam;
#endif
	struct CHIP_DBG_OPS *prChipDbg = NULL;
	struct STA_RECORD *prStaRec;
	int32_t i4Rssi0, i4Rssi1, i4BytesWritten;

	if (!prNetDev)
		return -1;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (!prGlueInfo)
		return -1;

	prAdapter = prGlueInfo->prAdapter;

	if (!prAdapter)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	ucBssIdx = wlanGetBssIdx(prNetDev);
	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
		!IS_BSS_INDEX_AIS(prAdapter, ucBssIdx) ||
		ucBssIdx >= BSSID_NUM )
		return -EINVAL;

	if (kalGetMediaStateIndicated(prGlueInfo, ucBssIdx) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
		return -EINVAL;
	}

#if (CFG_SUPPORT_STATS_ONE_CMD == 1)
	rParam.u4Period = SEC_TO_MSEC(CFG_LQ_MONITOR_FREQUENCY);
	rStatus = kalIoctlByBssIdx(prGlueInfo,
		   wlanoidQueryStatsOneCmd, &rParam,
		   sizeof(rParam), &u4BufLen, ucBssIdx);
	DBGLOG(REQ, TRACE,
			"rStatus=%u", rStatus);
#endif

	kalMemZero(rsp, sizeof(rsp));

	/* RSSI */
	index += kalSprintf(rsp + index, "rssi:%ddBm",
		prGlueInfo->i4RssiCache[ucBssIdx]);
	DBGLOG(OID, TRACE, "RSSI =%d\n", prGlueInfo->i4RssiCache[ucBssIdx]);

	prChipDbg = prAdapter->chip_info->prDebugOps;
	if (prChipDbg && prChipDbg->get_rssi_from_wtbl) {
		prStaRec = aisGetStaRecOfAP(prAdapter, ucBssIdx);
		if (prStaRec) {
			prChipDbg->get_rssi_from_wtbl(prAdapter,
				prStaRec->ucWlanIndex, &i4Rssi0, &i4Rssi1,
				NULL, NULL);
			index += kalSprintf(rsp + index,
				" ant1rssi:%ddBm ant2rssi:%ddBm",
				i4Rssi0, i4Rssi1);
			DBGLOG(REQ, ERROR, "ant1rssi:%ddBm ant2rssi:%ddB",
					i4Rssi0, i4Rssi1);
		}
	}

    /* packets and bytes*/
	prDevStats = (struct net_device_stats *)kalGetStats(prNetDev);

	if (prDevStats) {
		index += kalSprintf(rsp + index,
			" rxpackets:%lu txpackets:%lu",
			prDevStats->rx_packets, prDevStats->tx_packets);
		DBGLOG(OID, TRACE, "rxpackets:%u txpackets:%u\n",
			prDevStats->rx_packets, prDevStats->tx_packets);

		index += kalSprintf(rsp + index,
			" rxbytes:%lu txbytes:%lu",
			prDevStats->rx_bytes, prDevStats->tx_bytes);
		DBGLOG(OID, TRACE, "rxbytes:%u txbytes:%u\n",
			prDevStats->rx_bytes, prDevStats->tx_bytes);
	}

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidGetStaDump,
			(void *)(rsp + index), 0, &u4BufLen, ucBssIdx);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	DBGLOG(REQ, INFO, "idx:%u buflen:%u\n", index, u4BufLen);
	i4BytesWritten = kalSnprintf(
		pcCommand, u4BufLen + index, "%s", rsp);

	DBGLOG(REQ, INFO,
		"command result is %s\n", pcCommand);

	return i4BytesWritten;
}

#if CFG_SUPPORT_BW_SELECT
int testmode_set_max_bw(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t u4BufLen;
	int32_t i4Ret = -1;
	uint32_t u4LeakyAP = 0;
	uint32_t u4MaxBw = 0;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rConfig = {0};
	uint8_t cmd[128] = {0};
	uint8_t strLen = 0;
	uint8_t ucBssIdx = 0;


	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo)
		prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -EINVAL;
	ucBssIdx = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIdx))
		return -EINVAL;

	rStatus=wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "parse Arg failed, %u", rStatus);
		return rStatus;
	}

	i4Ret = kalkStrtou32(apcArgv[1], 0, &u4LeakyAP);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "parse u4LeakyAP error %d\n", i4Ret);
		return WLAN_STATUS_INVALID_DATA;
	}

	i4Ret = kalkStrtou32(apcArgv[2], 0, &u4MaxBw);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "parse u4MaxBw error %d\n", i4Ret);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (i4Argc > 3) {
		DBGLOG(REQ, ERROR, "Error input parameters(%d):%s\n", i4Argc, pcCommand);
		return WLAN_STATUS_INVALID_DATA;
	}

	strLen = kalSnprintf(cmd, sizeof(cmd), "setMaxBw %d", u4MaxBw);

	DBGLOG(REQ, TRACE, "set_chip to FW %s strlen=%d\n", cmd, strLen);

	rConfig.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
	rConfig.u2MsgSize = strLen;
	kalStrnCpy(rConfig.aucCmd, cmd, strLen);

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetChipConfig,
		(void *)&rConfig,
		sizeof(struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT), &u4BufLen,
		ucBssIdx);

	return rStatus;

}


int testmode_get_max_bw(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)

{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t u4BufLen;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rConfig = {0};
	uint8_t cmd[128] = {0};
	uint8_t strLen = 0;
	uint8_t ucBssIdx = 0;
	uint8_t strEventLen = 0;
	uint8_t returnevent[128] = {0};
	uint32_t u4MaxBw = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo)
		prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -EINVAL;
	ucBssIdx = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIdx))
		return -EINVAL;

	rStatus=wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "parse Arg failed, %u", rStatus);
		return rStatus;
	}

	strLen = kalSnprintf(cmd, sizeof(cmd), "TxMaxBw");

	rConfig.ucType = CHIP_CONFIG_TYPE_ASCII;
	rConfig.u2MsgSize = strLen;
	kalStrnCpy(rConfig.aucCmd, cmd, strLen);

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidQueryChipConfig,
		(void *)&rConfig,
		sizeof(struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT), &u4BufLen,
		ucBssIdx);

	if (rStatus != WLAN_STATUS_SUCCESS)
	{
		DBGLOG(REQ, ERROR, "ioctrl failed=%d\n", rStatus);
		return -1;
	}

	kalMemZero(cmd, sizeof(cmd));
	strLen = kalSnprintf(cmd, sizeof(cmd), "TxMaxBw = ");

	if (kalStrnCmp(rConfig.aucCmd, cmd, strLen) == 0) {
		kalkStrtou32(rConfig.aucCmd + strLen, 0, &u4MaxBw);
		DBGLOG(REQ, TRACE, "in str cmp: %d\n", u4MaxBw);
	}

	strEventLen = kalSnprintf(returnevent, sizeof(returnevent), "%d %d\n",
		LEAKY_AP_DETECT, u4MaxBw);
	/* only support leaky AP detect equal to 1, should not send OMN */

	DBGLOG(REQ, TRACE, "get_chip from FW %s strlen=%d\n", returnevent, strEventLen);

	return mtk_cfg80211_process_str_cmd_reply(wiphy, returnevent, strEventLen);

}
#endif

int priv_driver_get_ap_maxnss(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	uint8_t ucNssNum = 0;
	uint8_t ucBssIndex = 0;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		goto error;

	ucBssIndex = wlanGetBssIdx(prNetDev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex)) {
		DBGLOG(REQ, WARN,
			"bss [%d] is invalid!\n", ucBssIndex);
		goto error;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	if (prAdapter->rWifiVar.ucAp6gNSS == 1 ||
		prAdapter->rWifiVar.ucAp5gNSS == 1 ||
		prAdapter->rWifiVar.ucAp2gNSS == 1)
		ucNssNum = 1;
	else
		ucNssNum = 2;

	i4BytesWritten = kalSnprintf(
		pcCommand, i4TotalLen, "%d", ucNssNum);
	DBGLOG(REQ, INFO,
		"command result is %s\n", pcCommand);

	return i4BytesWritten;
error:
	return -1;

}

int priv_driver_set_ap_maxnss(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucNSS = 0;
	uint8_t ucBssIndex = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo)
		return -1;
	prAdapter = prGlueInfo->prAdapter;
	if (!prAdapter)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	ucBssIndex = wlanGetBssIdx(prNetDev);
	if (ucBssIndex >= BSSID_NUM)
		return -EFAULT;

	prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
	if (!prBssInfo)
		return -EFAULT;

	if (i4Argc >= 1) {
		u4Ret = kalkStrtou32(apcArgv[i4Argc - 1], 0, &u4Parse);
		if (u4Ret) {
			DBGLOG(REQ, WARN, "parse apcArgv error u4Ret=%d\n",
				u4Ret);
			goto error;
		}
		ucNSS = (uint8_t) u4Parse;

		if ((ucNSS > 0) &&
			(ucNSS <= prAdapter->rWifiVar.ucNSS)) {

			prAdapter->rWifiVar.ucAp6gNSS = ucNSS;
			prAdapter->rWifiVar.ucAp5gNSS = ucNSS;
			prAdapter->rWifiVar.ucAp2gNSS = ucNSS;
			DBGLOG(REQ, STATE,
				"ApNSS = %d\n",
				prAdapter->rWifiVar.ucAp2gNSS);
	} else
		DBGLOG(REQ, WARN, "Invalid nss=%d\n",
			ucNSS);
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX AP_SET_NSS <value>\n");
	}

	return i4BytesWritten;
error:
	return -1;

}

int priv_driver_set_ap_axmode(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
#if (CFG_SUPPORT_802_11AX == 1)
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucMode;
	uint8_t ucApHe;
	uint8_t ucApEht;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (prAdapter && (i4Argc >= 1)) {
		u4Ret = kalkStrtou32(apcArgv[i4Argc - 1], 0, &u4Parse);
		if (u4Ret) {
			DBGLOG(REQ, WARN, "parse apcArgv error u4Ret=%d\n",
				u4Ret);
			goto error;
		}
		ucMode = (uint8_t) u4Parse;

		ucApHe = ucMode;
		if (ucApHe > FEATURE_DISABLED)
			ucApHe = FEATURE_FORCE_ENABLED;
		ucApEht = (ucMode >> 1);
		if (ucApEht > FEATURE_DISABLED)
			ucApEht = FEATURE_FORCE_ENABLED;

		DBGLOG(REQ, STATE, "ucApHe:%d, ucApEht: %d\n",
			ucApHe, ucApEht);

		prAdapter->rWifiVar.ucApHe = ucApHe;
#if (CFG_SUPPORT_802_11BE == 1)
		prAdapter->rWifiVar.ucApEht = ucApEht;
#endif
	} else {
		DBGLOG(INIT, ERROR,
		  "iwpriv apx HAPD_SET_AX_MODE <value>\n");
	}

	return i4BytesWritten;
error:
#endif

	return -1;
}

#if CFG_SUPPORT_P2P_ECSA
uint8_t _getBwForInt(uint32_t ch_bw)
{
	uint8_t bw = 0;

	switch (ch_bw) {
	case 20:
		bw = MAX_BW_20MHZ;
		break;
	case 40:
		bw = MAX_BW_40MHZ;
		break;
	case 80:
		bw = MAX_BW_80MHZ;
		break;
	default:
		bw = MAX_BW_UNKNOWN;
		break;
	}

	return bw;
}

int priv_driver_set_ecsa(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t ch = 0;
	uint32_t bw = 0;
	uint32_t u4Ret = 0;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;
	enum ENUM_BAND eBand = BAND_NULL;
	struct BSS_INFO *bss = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter,
		ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %i\n", i4Argc);

	if (i4Argc < 3) {
		DBGLOG(REQ, INFO, "Input insufficent\n");
		return -1;
	}

	bss =
		GET_BSS_INFO_BY_INDEX(
		prGlueInfo->prAdapter,
		ucBssIdx);
	if (bss == NULL)
		return -1;

	u4Ret = kalkStrtou32(apcArgv[1], 0, &ch);
	if (u4Ret) {
		DBGLOG(REQ, LOUD,
			"parse ch error u4Ret=%d\n",
			u4Ret);
		return -1;
	}
	u4Ret = kalkStrtou32(apcArgv[2], 0, &bw);
	if (u4Ret) {
		DBGLOG(REQ, WARN,
			"parse ch_bw error u4Ret=%d\n",
			u4Ret);
		return -1;
	} else if (bw != 20 && bw != 40 && bw != 80) {
		DBGLOG(REQ, WARN, "Incorrect bw\n");
		return -1;
	}

	eBand = (ch <= 14) ? BAND_2G4 : BAND_5G;

	bw = _getBwForInt(bw);
	if (bw != MAX_BW_UNKNOWN) {
		prGlueInfo->prAdapter
			->rWifiVar.prP2pSpecificBssInfo[ucRoleIdx]
			->ucEcsaBw = bw;
		prGlueInfo->prAdapter
			->rWifiVar.prP2pSpecificBssInfo[ucRoleIdx]
			->fgEcsa = TRUE;
	}

	if (IS_BSS_APGO(bss))
		u4Ret = cnmIdcCsaReq(prGlueInfo->prAdapter,
			eBand, ch, ucRoleIdx);
	else
		DBGLOG(REQ, WARN, "Incorrect bss opmode\n");

	DBGLOG(REQ, INFO, "u4Ret is %d\n", u4Ret);

	return 0;
}
#endif

#if CFG_SUPPORT_TDLS
static int priv_driver_get_tdls_available(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	uint8_t fgAvailable = TRUE;
	uint8_t ucBssIndex = 0;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		goto error;

	ucBssIndex = wlanGetBssIdx(prNetDev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex)) {
		DBGLOG(REQ, WARN,
			"bss [%d] is invalid!\n", ucBssIndex);
		goto error;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	fgAvailable =
		TdlsEnabled(prAdapter) &&
		TdlsAllowed(prAdapter, ucBssIndex);

	i4BytesWritten = kalSnprintf(
		pcCommand, i4TotalLen, "%d", fgAvailable);

	DBGLOG(REQ, INFO,
		"command result is %s\n", pcCommand);

	return i4BytesWritten;

error:
	return -1;
}

static int priv_driver_get_tdls_wider_bw(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	uint8_t fgEnable = FALSE;
	uint8_t ucBssIndex = 0;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		goto error;

	ucBssIndex = wlanGetBssIdx(prNetDev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex)) {
		DBGLOG(REQ, WARN,
			"bss [%d] is invalid!\n", ucBssIndex);
		goto error;
	}

	prGlueInfo = *((struct GLUE_INFO **)
		netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	fgEnable =
		TdlsEnabled(prAdapter) &&
		TdlsNeedAdjustBw(prAdapter, ucBssIndex);

	i4BytesWritten = kalSnprintf(
		pcCommand, i4TotalLen, "%d", fgEnable);

	DBGLOG(REQ, INFO,
		"command result is %s\n", pcCommand);

	return i4BytesWritten;

error:
	return -1;
}

static int priv_driver_get_tdls_max_session(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = 0;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		goto error;

	i4BytesWritten = kalSnprintf(
		pcCommand, i4TotalLen, "%d", MAXNUM_TDLS_PEER);

	DBGLOG(REQ, INFO,
		"command result is %s\n", pcCommand);

	return i4BytesWritten;

error:
	return -1;
}

static int priv_driver_get_tdls_num_of_session(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		goto error;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	if (!prAdapter)
		goto error;

	i4BytesWritten = kalSnprintf(
		pcCommand, i4TotalLen, "%d",
		prAdapter->u4TdlsLinkCount);

	DBGLOG(REQ, INFO,
		"command result is %s\n", pcCommand);

	return i4BytesWritten;

error:
	return -1;
}

static int priv_driver_set_tdls_enabled(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucEnabled;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 1) {
		u4Ret = kalkStrtou32(apcArgv[i4Argc - 1], 0, &u4Parse);
		if (u4Ret) {
			DBGLOG(REQ, WARN, "parse apcArgv error u4Ret=%d\n",
				u4Ret);
			goto error;
		}
		ucEnabled = (uint8_t) u4Parse;

		if (ucEnabled)
			prGlueInfo->prAdapter->rWifiVar.fgTdlsDisable = 0;
		else
			prGlueInfo->prAdapter->rWifiVar.fgTdlsDisable = 1;

		DBGLOG(REQ, STATE,
			"fgTdlsDisable = %d\n",
			prGlueInfo->prAdapter->rWifiVar.fgTdlsDisable);
	} else {
		DBGLOG(INIT, ERROR,
		  "iwpriv apx SET_TDLS_ENABLED <value>\n");
	}

	return i4BytesWritten;

error:
	return -1;
}
#endif

#if CFG_SUPPORT_IDC_CH_SWITCH
int priv_driver_set_idc_ril_bridge(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Channel = 0;
	uint32_t u4Band = 0;
	uint32_t u4Ret = 0;
	uint32_t ucRat = 0;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter,
		ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %i\n", i4Argc);

	if (i4Argc >= 4) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &ucRat);
		u4Ret = kalkStrtou32(apcArgv[2], 0, &u4Band);
		u4Ret = kalkStrtou32(apcArgv[3], 0, &u4Channel);

		DBGLOG(REQ, INFO, "u4Ret is %d\n", u4Ret);
		DBGLOG(REQ, INFO,
			"Update CP channel info [%d,%d,%d]\n",
			ucRat,
			u4Band,
			u4Channel);

#if CFG_SUPPORT_IDC_RIL_BRIDGE
		kalSetRilBridgeChannelInfo(
			prGlueInfo->prAdapter,
			ucRat,
			u4Band,
			u4Channel);
#endif
	} else {
		DBGLOG(REQ, INFO, "Input insufficent\n");
	}

	return 0;
}
#endif

#ifdef CFG_SUPPORT_UWB_COEX
#if CFG_SUPPORT_UWB_COEX
static uint32_t g_uwbcx_enable;
static uint32_t g_uwbcx_startch;
static uint32_t g_uwbcx_endch;
static uint32_t g_uwbcx_prepare = 10;

static void _setUwbCoexEnable(
	struct ADAPTER *prAdapter,
	uint32_t u4Enable,
	uint32_t u4StartCh,
	uint32_t u4EndCh)
{
	struct CMD_SET_UWB_COEX_ENABLE *prCmd;
	uint16_t u2CmdBufLen = 0;

	do {
		if (!prAdapter)
			break;

		u2CmdBufLen =
			sizeof(struct CMD_SET_UWB_COEX_ENABLE);

		prCmd = (struct CMD_SET_UWB_COEX_ENABLE *)
			cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
			u2CmdBufLen);
		if (!prCmd) {
			DBGLOG(P2P, ERROR,
				"cnmMemAlloc for prCmd failed!\n");
			break;
		}

		g_uwbcx_enable = u4Enable;
		g_uwbcx_startch = u4StartCh;
		g_uwbcx_endch = u4EndCh;

		prCmd->u4Enable = u4Enable;
		prCmd->u4StartCh = u4StartCh;
		prCmd->u4EndCh = u4EndCh;

		DBGLOG(REQ, INFO,
			"Enable, startch, endch = [%d,%d,%d]\n",
			u4Enable,
			u4StartCh,
			u4EndCh);

		wlanSendSetQueryCmd(prAdapter,
			CMD_ID_SET_UWB_COEX_ENABLE,
			TRUE,
			FALSE,
			FALSE,
			NULL,
			NULL,
			u2CmdBufLen,
			(uint8_t *) prCmd,
			NULL,
			0);

		cnmMemFree(prAdapter, prCmd);
	} while (FALSE);
}

static void _setUwbCoexPrepare(
	struct ADAPTER *prAdapter,
	uint32_t u4Time)
{
	struct CMD_SET_UWB_COEX_PREPARE *prCmd;
	uint16_t u2CmdBufLen = 0;

	do {
		if (!prAdapter)
			break;

		u2CmdBufLen =
			sizeof(struct CMD_SET_UWB_COEX_PREPARE);

		prCmd = (struct CMD_SET_UWB_COEX_PREPARE *)
			cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
			u2CmdBufLen);
		if (!prCmd) {
			DBGLOG(P2P, ERROR,
				"cnmMemAlloc for prCmd failed!\n");
			break;
		}

		g_uwbcx_prepare = u4Time;

		prCmd->u4Time = u4Time;

		DBGLOG(REQ, INFO, "Prepare time = %d\n", u4Time);

		wlanSendSetQueryCmd(prAdapter,
			CMD_ID_SET_UWB_COEX_PREPARE,
			TRUE,
			FALSE,
			FALSE,
			NULL,
			NULL,
			u2CmdBufLen,
			(uint8_t *) prCmd,
			NULL,
			0);

		cnmMemFree(prAdapter, prCmd);
	} while (FALSE);

}

int priv_driver_set_uwbcx_enable(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t enable = 0;
	uint32_t startch = 0;
	uint32_t endch = 0;
	uint32_t u4Ret = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo)
		return -1;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %i\n", i4Argc);

	if (i4Argc >= 4) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &enable);
		u4Ret = kalkStrtou32(apcArgv[2], 0, &startch);
		u4Ret = kalkStrtou32(apcArgv[3], 0, &endch);

		DBGLOG(REQ, LOUD, "u4Ret is %d\n", u4Ret);

		_setUwbCoexEnable(
			prGlueInfo->prAdapter,
			enable,
			startch,
			endch);
	} else {
		DBGLOG(REQ, INFO, "Input insufficent\n");
	}

	return 0;
}

int priv_driver_set_uwbcx_prepare(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t t = 0;
	uint32_t u4Ret = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo)
		return -1;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &t);

		DBGLOG(REQ, LOUD, "u4Ret is %d\n", u4Ret);

		_setUwbCoexPrepare(
			prGlueInfo->prAdapter, t);
	} else {
		DBGLOG(REQ, INFO, "Input insufficent\n");
	}

	return 0;
}

int priv_driver_get_uwbcx_enable(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	if (!prGlueInfo) {
		LOGBUF(pcCommand, i4TotalLen,
			i4BytesWritten, "Not Supported.");
		return i4BytesWritten;
	}

	LOGBUF(pcCommand,
		i4TotalLen,
		i4BytesWritten,
		"\n%d %d %d",
		g_uwbcx_enable,
		g_uwbcx_startch,
		g_uwbcx_endch);

	return	i4BytesWritten;
}

int priv_driver_get_uwbcx_prepare(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	if (!prGlueInfo) {
		LOGBUF(pcCommand, i4TotalLen,
			i4BytesWritten, "Not Supported.");
		return i4BytesWritten;
	}

	LOGBUF(pcCommand,
		i4TotalLen,
		i4BytesWritten,
		"\n%d",
		g_uwbcx_prepare);

	return	i4BytesWritten;
}
#endif
#endif

#if CFG_SAP_RPS_SUPPORT
int priv_driver_enable_sap_rps(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0, i4BytesWritten = 0;
	u_int8_t fgSetSapRps;



	DBGLOG(REQ, ERROR, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, ERROR, "argc is %i\n", i4Argc);

	if (i4Argc < 2)
		return -1;

	kalkStrtou8(apcArgv[1], 0, &fgSetSapRps);

	if (fgSetSapRps > SAP_RPS_ENABLE_TRUE)
		return -1;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **)
		netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	prAdapter->rWifiVar.fgSapRpsEnable = fgSetSapRps;
	prAdapter->rWifiVar.u4RpsMeetTime = 0;

	DBGLOG(REQ, LOUD, "fgSapRpsEnable %u\n",
		prAdapter->rWifiVar.fgSapRpsEnable);


	return i4BytesWritten;
}

int priv_driver_query_rps(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);


	i4BytesWritten += kalSnprintf(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\n[RPS Info]\n");
	i4BytesWritten += kalSnprintf(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"RpsInpktThresh : %u\n",
		prAdapter->rWifiVar.u4RpsInpktThresh);
	i4BytesWritten += kalSnprintf(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"SapRpsPhase : %u\n",
		prAdapter->rWifiVar.ucSapRpsPhase);
	i4BytesWritten += kalSnprintf(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"RpsTriggerTime : %u\n",
		prAdapter->rWifiVar.u4RpsTriggerTime);
	i4BytesWritten += kalSnprintf(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"SapRpsStatus : %u\n",
		prAdapter->rWifiVar.ucSapRpsStatus);
	i4BytesWritten += kalSnprintf(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"SapRpsEnable : %u\n",
		prAdapter->rWifiVar.fgSapRpsEnable);
	i4BytesWritten += kalSnprintf(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"SapRpsSwitch : %u\n",
		prAdapter->rWifiVar.fgSapRpsSwitch);
	i4BytesWritten += kalSnprintf(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"RpsMeetTime : %u\n",
		prAdapter->rWifiVar.u4RpsMeetTime);
	i4BytesWritten += kalSnprintf(
		pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"SapRpsForceOn : %u\n",
		prAdapter->rWifiVar.fgSapRpsForceOn);

	return i4BytesWritten;
}

/*----------------------------------------------------------------------------*/
/*
 * @ The function will force enable/ disable SAP RPS functionality.
 *  example: iwpriv ap0 driver "SET_SAP_RPS 1 1"
 */
/*----------------------------------------------------------------------------*/
static int priv_driver_set_sap_rps(struct net_device *prNetDev,
				     char *pcCommand, int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0, i4BytesWritten = 0;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;
	u_int8_t fgSetSapRps;
	uint8_t  ucSapRpsPhase;


	DBGLOG(REQ, ERROR, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, ERROR, "argc is %i\n", i4Argc);

	if (i4Argc < 3)
		return -1;

	kalkStrtou8(apcArgv[1], 0, &fgSetSapRps);
	kalkStrtou8(apcArgv[2], 0, &ucSapRpsPhase);

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	DBGLOG(REQ, LOUD, "ucRoleIdx %hhu ucBssIdx %hhu\n", ucRoleIdx,
	       ucBssIdx);
	prAdapter->rWifiVar.fgSapRpsForceOn = fgSetSapRps;

	p2pSetSapRps(prAdapter, fgSetSapRps, ucSapRpsPhase, ucBssIdx);

	return i4BytesWritten;
} /* priv_driver_add_acl_entry */

#endif

#if CFG_SAP_SUS_SUPPORT
/*----------------------------------------------------------------------------*/
/*
 * @ The function will force enable/ disable SAP RPS functionality.
 *  example: iwpriv ap0 driver "SET_SAP_RPS 1 1"
 */
/*----------------------------------------------------------------------------*/
static int priv_driver_set_sap_sus(struct net_device *prNetDev,
				     char *pcCommand, int i4TotalLen)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPriv;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0;
	u_int8_t fgSetSapSus;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prNetDevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		netdev_priv(prNetDev);

	prGlueInfo = prNetDevPriv->prGlueInfo;
	if (prGlueInfo->u4ReadyFlag == 0)
		return -1;

	prAdapter = prGlueInfo->prAdapter;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	kalkStrtou8(apcArgv[1], 0, &fgSetSapSus);

	prAdapter->rWifiVar.fgSapRpsForceOn = 0;
	p2pSetSapRps(prAdapter, 0, SAP_RPS_PHASE_MIN, prNetDevPriv->ucBssIdx);

	prAdapter->rWifiVar.fgSapSuspendOn = fgSetSapSus;
	p2pSetSapSus(prAdapter, prNetDevPriv->ucBssIdx, fgSetSapSus);

	return 0;
} /* priv_driver_set_sap_sus */
#endif

#if CFG_SUPPORT_NCHO
int priv_driver_get_band(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t i4BytesWritten = 0;
	uint32_t u4Band = 0;
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoBand,
			   &u4Band, sizeof(u4Band), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	i4BytesWritten = kalSnprintf(pcCommand,
				     i4TotalLen,
				     CMD_GETBAND" %u",
				     u4Band);

	return i4BytesWritten;
}
#endif

struct PRIV_CMD_HANDLER priv_cmd_ext_handlers[] = {
	{
		.pcCmdStr  = CMD_SET_HAPD_AXMODE,
		.pfHandler = priv_driver_set_ap_axmode,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = set_ext_policy,
		.u4PolicySize = ARRAY_SIZE(set_ext_policy)
	},
	{
		.pcCmdStr  = GET_HOTSPOT_ANTENNA_MODE,
		.pfHandler = priv_driver_get_ap_maxnss,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = SET_HOTSPOT_ANTENNA_MODE,
		.pfHandler = priv_driver_set_ap_maxnss,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = set_ext_policy,
		.u4PolicySize = ARRAY_SIZE(set_ext_policy)
	},
#if CFG_SUPPORT_P2P_ECSA
	{
		.pcCmdStr  = CMD_ECSA,
		.pfHandler = priv_driver_set_ecsa,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = set_ecsa_policy,
		.u4PolicySize = ARRAY_SIZE(set_ecsa_policy)
	},
#endif
#if CFG_SUPPORT_TDLS
	{
		.pcCmdStr  = CMD_GET_TDLS_AVAILABLE,
		.pfHandler = priv_driver_get_tdls_available,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_GET_TDLS_WIDER_BW,
		.pfHandler = priv_driver_get_tdls_wider_bw,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_GET_TDLS_MAX_SESSION,
		.pfHandler = priv_driver_get_tdls_max_session,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_GET_TDLS_NUM_OF_SESSION,
		.pfHandler = priv_driver_get_tdls_num_of_session,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_SET_TDLS_ENABLED,
		.pfHandler = priv_driver_set_tdls_enabled,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
#endif

#if CFG_SUPPORT_IDC_CH_SWITCH
	{
		.pcCmdStr  = CMD_SET_IDC_RIL,
		.pfHandler = priv_driver_set_idc_ril_bridge,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(4),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
#endif
#ifdef CFG_SUPPORT_UWB_COEX
#if CFG_SUPPORT_UWB_COEX
	{
		.pcCmdStr  = CMD_SET_UWBCX_ENABLE,
		.pfHandler = priv_driver_set_uwbcx_enable,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(4),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_SET_UWBCX_PREPARE_TIME,
		.pfHandler = priv_driver_set_uwbcx_prepare,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_GET_UWBCX_ENABLE,
		.pfHandler = priv_driver_get_uwbcx_enable,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_GET_UWBCX_PREPARE_TIME,
		.pfHandler = priv_driver_get_uwbcx_prepare,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
#endif
#endif
#if CFG_SAP_RPS_SUPPORT
	{
		.pcCmdStr  = CMD_ENABLE_SAP_RPS,
		.pfHandler = priv_driver_enable_sap_rps,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = rps_enable_policy
	},
	{
		.pcCmdStr  = CMD_QUERY_RPS,
		.pfHandler = priv_driver_query_rps,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL
	},
	{
		.pcCmdStr  = CMD_FORCE_SAP_RPS,
		.pfHandler = priv_driver_set_sap_rps,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = rps_force_set_policy
	},
#endif
#if CFG_SAP_SUS_SUPPORT
	{
		.pcCmdStr  = CMD_FORCE_SAP_SUS,
		.pfHandler = priv_driver_set_sap_sus,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = sap_sus_policy
	},
#endif
#if 0
	{
		.pcCmdStr  = CMD_GET_STA_TRX_INFO,
		.pfHandler = priv_driver_get_sta_trx_info,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = sta_info_policy,
		.u4PolicySize = ARRAY_SIZE(sta_info_policy)
	},
#endif
	{
		.pcCmdStr  = CMD_GET_STA_DUMP,
		.pfHandler = priv_driver_get_sta_dump,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
#if CFG_SUPPORT_NCHO
	{
		.pcCmdStr  = CMD_GETBAND,
		.pfHandler = priv_driver_get_band,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
#endif
};

#if WLAN_INCLUDE_SYS
int testmode_wifi_fcc_channel(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	int32_t i4BytesWritten = -1;
	int32_t ucType = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_WLAN_ON |
				WLAN_DRV_READY_CHECK_RESET)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc %i, cmd [%s]\n", i4Argc, apcArgv[1]);
		i4BytesWritten = kstrtos32(apcArgv[1], 0, &ucType);

		if (i4BytesWritten)
			DBGLOG(REQ, ERROR, "parse ucType error %d\n",
					i4BytesWritten);

#if (CFG_SUPPORT_WIFI_6G == 1)
		if (ucType == -1 || ucType == 1) {
			DBGLOG(REQ, INFO,
				"Enanble 6G cap, ucType: %d , 6g disallow option: %d\n",
				ucType, prGlueInfo->prAdapter->rWifiVar.ucDisallowBand6G);
			prGlueInfo->prAdapter->fgIsHwSupport6G = TRUE;
			prGlueInfo->prAdapter->rWifiVar.ucDisallowBand6G = 0;
			wlanCfgSetUint32(prGlueInfo->prAdapter,
				"DisallowBand6G", 0);
#if CFG_SUPPORT_NAN
			prGlueInfo->prAdapter->rWifiVar.ucNanEnable6g = 1;
			wlanCfgSetUint32(prGlueInfo->prAdapter,
				"NanEnable6g", 1);
			prGlueInfo->prAdapter->rWifiVar.ucNanEnableSS6g = 1;
			wlanCfgSetUint32(prGlueInfo->prAdapter,
				"NanEnableSS6g", 1);
			nanSet6gConfig(prGlueInfo->prAdapter);
			rStatus = WLAN_STATUS_SUCCESS;
#endif
		} else if (ucType == 0 &&
			prGlueInfo->prAdapter->fgEnRfTestMode == FALSE) {
			DBGLOG(REQ, INFO,
				"Disable 6G cap, ucType: %d, 6g disallow option: %d\n",
				ucType, prGlueInfo->prAdapter->rWifiVar.ucDisallowBand6G);
			prGlueInfo->prAdapter->fgIsHwSupport6G = FALSE;
			prGlueInfo->prAdapter->rWifiVar.ucDisallowBand6G = 1;
			wlanCfgSetUint32(prGlueInfo->prAdapter,
				"DisallowBand6G", 1);
#if CFG_SUPPORT_NAN
			prGlueInfo->prAdapter->rWifiVar.ucNanEnable6g = 0;
			wlanCfgSetUint32(prGlueInfo->prAdapter,
				"NanEnable6g", 0);
			prGlueInfo->prAdapter->rWifiVar.ucNanEnableSS6g = 0;
			wlanCfgSetUint32(prGlueInfo->prAdapter,
				"NanEnableSS6g", 0);
			nanSet6gConfig(prGlueInfo->prAdapter);
			rStatus = WLAN_STATUS_SUCCESS;
#endif
		}

		/* Update supported channel list in channel table based on current
		 * country domain
		 */
		rlmDomainSendCmd(prGlueInfo->prAdapter, TRUE);
		wlanUpdateChannelTable(prGlueInfo);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
#else
		DBGLOG(REQ, ERROR, "invalid cmd due to no support for 6G\n");
			return WLAN_STATUS_FAILURE;
#endif
	} else {
		DBGLOG(REQ, ERROR, "fail invalid data\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_wifi_safe_mode(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	int32_t i4BytesWritten = -1;
	uint8_t ucType = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);

	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHECK_WLAN_ON |
				WLAN_DRV_READY_CHECK_RESET)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc %i, cmd [%s]\n", i4Argc, apcArgv[1]);
		i4BytesWritten = kalkStrtou8(apcArgv[1], 0, &ucType);
		if (i4BytesWritten)
			DBGLOG(REQ, ERROR, "parse ucType error %d\n",
					i4BytesWritten);

#if (CFG_SUPPORT_WIFI_6G == 1)
		/* SETWSECINFO 1 or 0 */
		if (ucType == 1) {
			prGlueInfo->prAdapter->fgEn6eSafeMode = TRUE;
			prGlueInfo->prAdapter->fg6eOffSpecNotShow = FALSE;
			rStatus = rsnSetCountryCodefor6g(prGlueInfo->prAdapter, 1);
		} else if (ucType == 0) {
			prGlueInfo->prAdapter->fgEn6eSafeMode = FALSE;
			rStatus = rsnSetCountryCodefor6g(prGlueInfo->prAdapter, 0);

			if (prGlueInfo->prAdapter->fgEnRfTestMode == FALSE) {
				prGlueInfo->prAdapter->fg6eOffSpecNotShow = TRUE;
				DBGLOG(RSN, INFO, "Filter out out-of-spec 6G AP\n");
			}
		}

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
#else
		DBGLOG(REQ, ERROR, "invalid cmd due to no support for 6G\n");
		return WLAN_STATUS_FAILURE;
#endif
	} else {
		DBGLOG(REQ, ERROR, "fail invalid data\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}
#endif

#if CFG_SUPPORT_SCAN_LOG

int
priv_driver_set_beacon_recv(struct wiphy *wiphy, uint8_t enable)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t reportBeaconEn;
	uint32_t u4BufLen;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR,
			"[SWIPS] Failed to get prGlueInfo from wiphy\n");
		return -EINVAL;
	}

	reportBeaconEn = enable;

	return kalIoctl(prGlueInfo,
		wlanoidSetBeaconRecv,
		&reportBeaconEn, sizeof(uint8_t),
		&u4BufLen);
}

int testmode_set_bcn_recv_start(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct AIS_FSM_INFO *prAisFsmInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIndex = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);

	prAisFsmInfo = aisGetAisFsmInfo(
		prGlueInfo->prAdapter,
		ucBssIndex);

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* return abort reason if connecting */
	if (prAisFsmInfo->eCurrentState == AIS_STATE_SEARCH ||
		prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN ||
		prAisFsmInfo->eCurrentState == AIS_STATE_JOIN) {
		DBGLOG(INIT, WARN,
			   "Dont start SWIPS when connecting\n");

		return mtk_cfg80211_process_str_cmd_reply(
			wiphy, "FAIL_CONNECT_STARTS",
			strlen("FAIL_CONNECT_STARTS") + 1);
	}

	/* return abort reason if connecting */
	if (prAisFsmInfo->eCurrentState == AIS_STATE_SCAN ||
		prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN ||
		prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR) {
		DBGLOG(INIT, WARN,
			"Dont start SWIPS when scanning\n");

		return mtk_cfg80211_process_str_cmd_reply(
			wiphy, "FAIL_SCAN_STARTS",
			strlen("FAIL_SCAN_STARTS") + 1);
	}

	rStatus = priv_driver_set_beacon_recv(wiphy, TRUE);

	DBGLOG(REQ, INFO, "rStatus=%d\n", rStatus);

	return rStatus;
}

int testmode_set_bcn_recv_stop(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	return priv_driver_set_beacon_recv(wiphy, FALSE);
}
#endif

#ifdef CFG_MTK_CONNSYS_DEDICATED_LOG_PATH
int testmode_set_debug_level(struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_CONNSYS_FW_LOG rFwLogCmd;
	uint32_t u4BufLen = 0;
	int32_t u4LogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;
	uint8_t ucInput;

	WIPHY_PRIV(wiphy, prGlueInfo);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(INIT, TRACE, "Set log level: %d\n", apcArgv[1]);

	kalkStrtou8(apcArgv[1], 0, &ucInput);

	kalMemZero(&rFwLogCmd, sizeof(rFwLogCmd));
	rFwLogCmd.fgEarlySet = FALSE;

	switch (ucInput) {
	case 1:
		DBGLOG(REQ, INFO, "DRV log: Default, FW log: OFF\n");

		u4LogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;
		wlanDbgSetLogLevelImpl(prGlueInfo->prAdapter,
			   ENUM_WIFI_LOG_LEVEL_VERSION_V1,
			   ENUM_WIFI_LOG_MODULE_DRIVER,
			   u4LogLevel);

		rFwLogCmd.fgCmd = FW_LOG_CMD_SET_LEVEL;
		rFwLogCmd.fgValue = u4LogLevel;
		kalIoctl(prGlueInfo,
			connsysFwLogControl,
			&rFwLogCmd,
			sizeof(struct CMD_CONNSYS_FW_LOG),
			&u4BufLen);

		rFwLogCmd.fgCmd = FW_LOG_CMD_ON_OFF;
		rFwLogCmd.fgValue = 0; /* OFF */
		kalIoctl(prGlueInfo,
			connsysFwLogControl,
			&rFwLogCmd,
			sizeof(struct CMD_CONNSYS_FW_LOG),
			&u4BufLen);

		rStatus = WLAN_STATUS_SUCCESS;

		break;
	case 2:
		DBGLOG(REQ, INFO, "DRV log: Default, FW log: Default\n");

		u4LogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;
		wlanDbgSetLogLevelImpl(prGlueInfo->prAdapter,
			   ENUM_WIFI_LOG_LEVEL_VERSION_V1,
			   ENUM_WIFI_LOG_MODULE_DRIVER,
			   u4LogLevel);

		rFwLogCmd.fgCmd = FW_LOG_CMD_ON_OFF;
		rFwLogCmd.fgValue = 1; /* ON */
		kalIoctl(prGlueInfo,
			connsysFwLogControl,
			&rFwLogCmd,
			sizeof(struct CMD_CONNSYS_FW_LOG),
			&u4BufLen);

		rFwLogCmd.fgCmd = FW_LOG_CMD_SET_LEVEL;
		rFwLogCmd.fgValue = u4LogLevel;
		kalIoctl(prGlueInfo,
			connsysFwLogControl,
			&rFwLogCmd,
			sizeof(struct CMD_CONNSYS_FW_LOG),
			&u4BufLen);

		rStatus = WLAN_STATUS_SUCCESS;

		break;
	case 3:
		DBGLOG(REQ, INFO, "DRV log: More, FW log: More\n");

		u4LogLevel = ENUM_WIFI_LOG_LEVEL_MORE;
		wlanDbgSetLogLevelImpl(prGlueInfo->prAdapter,
			   ENUM_WIFI_LOG_LEVEL_VERSION_V1,
			   ENUM_WIFI_LOG_MODULE_DRIVER,
			   u4LogLevel);

		rFwLogCmd.fgCmd = FW_LOG_CMD_ON_OFF;
		rFwLogCmd.fgValue = 1; /* ON */
		kalIoctl(prGlueInfo,
			connsysFwLogControl,
			&rFwLogCmd,
			sizeof(struct CMD_CONNSYS_FW_LOG),
			&u4BufLen);

		rFwLogCmd.fgCmd = FW_LOG_CMD_SET_LEVEL;
		rFwLogCmd.fgValue = u4LogLevel;
		kalIoctl(prGlueInfo,
			connsysFwLogControl,
			&rFwLogCmd,
			sizeof(struct CMD_CONNSYS_FW_LOG),
			&u4BufLen);

		 rStatus = WLAN_STATUS_SUCCESS;

		break;
	default:
		DBGLOG(REQ, WARN, "Invalid log level.\n");
		rStatus =  -EOPNOTSUPP;
		break;
	}

	return rStatus;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set Domain Info with blocked channels.
 *        wpa_supplicant uses SET_INDOOR_CHANNELS <channel num> <channels ...>
 *        to send channels it wants to block. After receiving these channels,
 *        we copy current domain info into blockedDomain and revise it. In our
 *        design, we update 6 sub domains to FW. Since the number of sub
 *        domains is only 6, we can only trim each sub domain from the begin or
 *        the end but divide one sub domain into two. For example, if we want
 *        to block <40> in sub domain <36,40,44,48>, it results in <36> because
 *        we don't have another sub domain to keep <44,48>.
 *
 * \param[in] prAdapter Pointer to the Adapter structure.
 * \param[in] pvSetBuffer A pointer to the buffer of blocked channels.
 * \param[in] u4SetBufferLen The length of the set buffer.
 * \param[out] pu4SetInfoLen If the call is successful, returns the number of
 *                           bytes read from the set buffer. If the call failed
 *                           due to invalid length of the set buffer, returns
 *                           the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanoidSetBlockIndoorChs(struct ADAPTER *prAdapter,
			 void *pvSetBuffer, uint32_t u4SetBufferLen,
			 uint32_t *pu4SetInfoLen)
{
	struct DOMAIN_INFO_ENTRY *prBlockedDomain;
	struct DOMAIN_SUBBAND_INFO *prSubBand;
	uint8_t *blocked, numBlocked;
	uint8_t newFirstCh, numNewDomain;
	uint8_t i, j, k, ch, isBlocked;

	prBlockedDomain = &prAdapter->rBlockedDomainInfo;

	/* Copy current domain info to modify */
	if (prAdapter->prDomainInfo == NULL) {
		DBGLOG(REQ, WARN,
		       "Cant block channels due to NULL prDomainInfo\n");
		return WLAN_STATUS_INVALID_DATA;
	}
	kalMemCopy(prBlockedDomain, prAdapter->prDomainInfo,
		   sizeof(struct DOMAIN_INFO_ENTRY));

	numBlocked = u4SetBufferLen;
	blocked = (uint8_t *) pvSetBuffer;

	for (i = 0; i < MAX_SUBBAND_NUM; i++) {
		numNewDomain = 0;
		newFirstCh = 0;
		prSubBand = &prBlockedDomain->rSubBand[i];
		if (prSubBand->ucBand != BAND_5G)
			continue;

		for (j = 0; j < prSubBand->ucNumChannels; j++) {
			ch = prSubBand->ucFirstChannelNum + 4 * j;
			isBlocked = FALSE;
			for (k = 0; k < numBlocked; k++) {
				if (ch == blocked[k]) {
					isBlocked = TRUE;
					break;
				}
			}
			if (isBlocked == FALSE && newFirstCh == 0) {
				newFirstCh = ch;
				numNewDomain++;
			} else if (isBlocked == FALSE)
				numNewDomain++;
			else if (isBlocked == TRUE && newFirstCh == 0)
				continue;
			else if (isBlocked == TRUE)
				break;
		}

		prSubBand->ucFirstChannelNum = newFirstCh;
		prSubBand->ucNumChannels = numNewDomain;
		DBGLOG(REQ, TRACE, "Set First ch = %d and num = %d\n",
		       prSubBand->ucFirstChannelNum, prSubBand->ucNumChannels);
	}

	rlmDomainSendDomainInfoCmd(prAdapter);
	wlanUpdateChannelTable(prAdapter->prGlueInfo);
	return WLAN_STATUS_SUCCESS;
}

int
testmode_set_indoor_ch(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t *blockedChs, numBlockedChs;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct REG_INFO *prRegInfo;
	uint32_t u4BufLen;
	int i, j;

	DBGLOG(INIT, TRACE, "SET_INDOOR_CHANNELS with cmd: %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR,
		       "Failed to get prGlueInfo from wiphy\n");
		return -EINVAL;
	}
	prAdapter = prGlueInfo->prAdapter;
	prRegInfo = &prGlueInfo->rRegInfo;

	if (kalkStrtou8(apcArgv[1], 0, &numBlockedChs))
		DBGLOG(REQ, LOUD, "parse %s error\n", apcArgv[1]);

	/* Reset domain info to country code default */
	if (numBlockedChs == 0) {
		uint8_t country[2] = {0};

		prRegInfo->eRegChannelListMap = REG_CH_MAP_COUNTRY_CODE;
		country[0] = (prAdapter->rWifiVar.u2CountryCode
				& 0xff00) >> 8;
		country[1] = prAdapter->rWifiVar.u2CountryCode
				& 0x00ff;

		return kalIoctl(prGlueInfo, wlanoidSetCountryCode,
				country, 2, &u4BufLen);
	}

	blockedChs = kalMemAlloc(sizeof(int8_t) * numBlockedChs,
				 VIR_MEM_TYPE);
	prRegInfo->eRegChannelListMap = REG_CH_MAP_BLOCK_INDOOR;
	for (i = 0, j = 0; i < numBlockedChs; i++) {
		if (kalkStrtou8(apcArgv[i + 2], 0, &(blockedChs[j])))
			DBGLOG(REQ, LOUD, "err parse ch %s\n", apcArgv[i + 2]);

		if (!kalIsValidChnl(prGlueInfo, blockedChs[j], BAND_5G)) {
			DBGLOG(INIT, INFO, "Skip blocked CH: %d\n",
				blockedChs[j]);
			continue;
		}

		j++;
	}
	numBlockedChs = j;

	return kalIoctl(prGlueInfo, wlanoidSetBlockIndoorChs,
			blockedChs, numBlockedChs, &u4BufLen);
}

#if CFG_SUPPORT_LLW_SCAN && (CFG_EXT_VERSION < 3)
uint32_t wlanoidSetScanParam(struct ADAPTER *prAdapter,
			    void *pvSetBuffer,
			    uint32_t u4SetBufferLen,
			    uint32_t *pu4SetInfoLen)
{
	struct PARAM_SCAN *param;
	struct AIS_FSM_INFO *ais;
	uint8_t ucBssIndex = 0;

	if (!prAdapter) {
		DBGLOG(REQ, ERROR, "prAdapter is NULL\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	if (!pvSetBuffer) {
		DBGLOG(REQ, ERROR, "pvGetBuffer is NULL\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	ais = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	param = (struct PARAM_SCAN *) pvSetBuffer;

	if (ais->ucLatencyCrtDataMode == 3) {
		DBGLOG(OID, INFO,
			"LATENCY_CRT_DATA = 3, not apply SET_DWELL_TIME\n");
		return WLAN_STATUS_SUCCESS;
	}

	if (param->ucDfsChDwellTimeMs != 0)
		ais->ucDfsChDwellTimeMs = param->ucDfsChDwellTimeMs;
	else
		ais->ucDfsChDwellTimeMs = 0;

	if (param->ucNonDfsChDwellTimeMs != 0)
		ais->ucNonDfsChDwellTimeMs = param->ucNonDfsChDwellTimeMs;
	else
		ais->ucNonDfsChDwellTimeMs = 0;

	ais->u2OpChStayTimeMs = param->u2OpChStayTimeMs;

	if (param->ucNonDfsChDwellTimeMs != 0) {
		ais->ucPerScanChannelCnt =
			param->u2OpChAwayTimeMs / param->ucNonDfsChDwellTimeMs;
	} else
		ais->ucPerScanChannelCnt = 0;

	DBGLOG(OID, INFO,
		"DFS(%d->%d), non-DFS(%d->%d), OpChTime(%d %d), PerScanCh(%d)\n",
		param->ucDfsChDwellTimeMs,
		ais->ucDfsChDwellTimeMs,
		param->ucNonDfsChDwellTimeMs,
		ais->ucNonDfsChDwellTimeMs,
		ais->u2OpChStayTimeMs,
		param->u2OpChAwayTimeMs,
		ais->ucPerScanChannelCnt);

	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanoidSetLatencyCrtData(struct ADAPTER *prAdapter,
			    void *pvSetBuffer,
			    uint32_t u4SetBufferLen,
			    uint32_t *pu4SetInfoLen)
{
	uint32_t *pu4Mode;
	struct AIS_FSM_INFO *ais;
	uint8_t ucBssIndex = 0;

	if (!prAdapter) {
		DBGLOG(REQ, ERROR, "prAdapter is NULL\n");
		return WLAN_STATUS_ADAPTER_NOT_READY;
	}

	if (!pvSetBuffer) {
		DBGLOG(REQ, ERROR, "pvGetBuffer is NULL\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	ais = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	pu4Mode = (uint32_t *) pvSetBuffer;

	ais->ucLatencyCrtDataMode = 0;
	/* Mode 2: Restrict full roam scan triggered by Firmware
	*          due to low RSSI.
	*  Mode 3: Restrict off channel time due to full scan to < 40ms
	*/
	ais->ucLatencyCrtDataMode = *pu4Mode;

	if (ais->ucLatencyCrtDataMode == 3) {
		ais->ucDfsChDwellTimeMs = 20;
		ais->ucNonDfsChDwellTimeMs = 35;
		ais->u2OpChStayTimeMs = 0;
		ais->ucPerScanChannelCnt = 1;
	} else if (ais->ucLatencyCrtDataMode == 0) {
		ais->ucDfsChDwellTimeMs = 0;
		ais->ucNonDfsChDwellTimeMs = 0;
		ais->u2OpChStayTimeMs = 0;
		ais->ucPerScanChannelCnt = 0;
	}

	return WLAN_STATUS_SUCCESS;
}

int testmode_set_scan_param(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4SetInfoLen = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_SCAN param;

	WIPHY_PRIV(wiphy, prGlueInfo);

	/*ex: wpa_cli driver SET_DWELL_TIME W X Y Z*/
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc != 5) {
		DBGLOG(REQ, ERROR,
			"Error input parameters(%d):%s\n", i4Argc, pcCommand);
		return WLAN_STATUS_INVALID_DATA;
	}

	kalMemZero(&param, sizeof(struct PARAM_SCAN));

	if (kalkStrtou8(apcArgv[1], 0, &param.ucDfsChDwellTimeMs)) {
		DBGLOG(REQ, LOUD, "DfsDwellTime parse %s err\n", apcArgv[1]);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (kalkStrtou16(apcArgv[2], 0, &param.u2OpChStayTimeMs)) {
		DBGLOG(REQ, LOUD, "OpChStayTimeMs parse %s err\n", apcArgv[2]);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (kalkStrtou8(apcArgv[3], 0, &param.ucNonDfsChDwellTimeMs)) {
		DBGLOG(REQ, LOUD,
			"NonDfsDwellTimeMs parse %s err\n", apcArgv[3]);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (kalkStrtou16(apcArgv[4], 0, &param.u2OpChAwayTimeMs)) {
		DBGLOG(REQ, LOUD, "OpChAwayTimeMs parse %s err\n", apcArgv[4]);
		return WLAN_STATUS_INVALID_DATA;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidSetScanParam,
			&param, sizeof(struct PARAM_SCAN),
			&u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR,
		       "SET_SCAN_PARAM fail 0x%x\n", rStatus);
	else
		DBGLOG(INIT, TRACE,
		       "SET_SCAN_PARAM successed\n");

	return rStatus;
}

int testmode_set_latency_crt_data(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0, i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4SetInfoLen = 0, u4Mode = 0;
	int8_t *pcmd = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	/* Since the pcCommand would be seperated by
	 * wlanCfgParseArgument, we should copy original pcCommand first.
	 */
	i4TotalLen = i4TotalLen + 1;
	pcmd = (int8_t*) kalMemAlloc(i4TotalLen, VIR_MEM_TYPE);
	kalMemZero(pcmd, i4TotalLen);
	kalMemCopy(pcmd, pcCommand, i4TotalLen);

	/*ex: wpa_cli driver SET_LATENCY_CRT_DATA X */
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc != 2) {
		DBGLOG(REQ, ERROR,
			"Error input parameters(%d):%s\n", i4Argc, pcCommand);
		if (pcmd)
			kalMemFree(pcmd, VIR_MEM_TYPE, i4TotalLen);
		return WLAN_STATUS_INVALID_DATA;
	}

	i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Mode);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "Set Latency crt mode parse error %d\n",
			i4Ret);
		if (pcmd)
			kalMemFree(pcmd, VIR_MEM_TYPE, i4TotalLen);
		return WLAN_STATUS_FAILURE;
	}

	/* Do further scan handling for mode 2 and mode 3,
	 * reset if u4Mode == 0
	 */
	if (u4Mode >= 2 || u4Mode == 0) {
		wlanChipConfigWithType(prGlueInfo->prAdapter,
			pcmd, 22, CHIP_CONFIG_TYPE_WO_RESPONSE);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetLatencyCrtData,
			&u4Mode, sizeof(uint32_t),
			&u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
				"SET_CRT_DATA fail 0x%x\n", rStatus);
		else
			DBGLOG(INIT, TRACE,
				"SET_CRT_DATA successed\n");
	} else {
		/* for mode 1 */
		wlanChipConfigWithType(prGlueInfo->prAdapter,
			pcmd, 22, CHIP_CONFIG_TYPE_WO_RESPONSE);
		rStatus = WLAN_STATUS_SUCCESS;
	}

	if (pcmd)
		kalMemFree(pcmd, VIR_MEM_TYPE, i4TotalLen);

	return rStatus;
}
#endif

int testmode_set_ap_sus(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
#if CFG_SAP_SUS_SUPPORT
	if (!wiphy || !wdev)
		return -EINVAL;

	return priv_driver_set_sap_sus(wdev->netdev, pcCommand, i4TotalLen);
#else
	DBGLOG(REQ, WARN, "not support\n");
	return -EOPNOTSUPP;
#endif
}

int testmode_get_sta_trx_info(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
#if CFG_STAINFO_FEATURE
	if (!wiphy || !wdev)
		return -EINVAL;

	return priv_driver_get_sta_trx_info(wdev->netdev, pcCommand, i4TotalLen);
#else
	DBGLOG(REQ, WARN, "not support\n");
	return -EOPNOTSUPP;
#endif
}

int testmode_set_ap_rps(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
#if CFG_SAP_RPS_SUPPORT
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct ADAPTER *prAdapter = NULL;
	u_int8_t fgSetSapRps = 0;
	u_int8_t ucBssIdx, ucRoleIdx;
	OS_SYSTIME now;
	OS_SYSTIME period;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (!prGlueInfo)
		return -EOPNOTSUPP;

	DBGLOG(REQ, WARN, "SET_AP_RPS cmd check ok\n");
	kalkStrtou8(apcArgv[1], 0, &fgSetSapRps);

	prAdapter = prGlueInfo->prAdapter;
	if (!prAdapter ||
		prAdapter->rWifiVar.fgSapSuspendOn == 1)
		return -EOPNOTSUPP;

	GET_BOOT_SYSTIME(&now);
	period = now;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, wdev->netdev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;
	prP2pBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	prAdapter->rWifiVar.fgSapRpsEnable = fgSetSapRps;
	if (fgSetSapRps == 0) {
		prAdapter->rWifiVar.u4RpsStopTime = now;
		p2pSetSapRps(prAdapter, fgSetSapRps, 3, ucBssIdx);
	} else if (fgSetSapRps == 1) {
		period =
			(period -
			prAdapter->rWifiVar.u4RpsStopTime) *
			MSEC_PER_SEC / KAL_HZ;
		DBGLOG(REQ, LOUD, "rps stop period %u and ap state %u\n",
			period, prP2pBssInfo->eConnectionState);
		if ((period <= 5000 &&
			(prAdapter->rWifiVar.u4RpsMeetTime >=
			prAdapter->rWifiVar.u4RpsTriggerTime)) ||
			(prP2pBssInfo->eConnectionState ==
			MEDIA_STATE_DISCONNECTED))
			p2pSetSapRps(prAdapter, TRUE,
				prAdapter->rWifiVar.ucSapRpsPhase,
				ucBssIdx);
		else
			prAdapter->rWifiVar.u4RpsMeetTime = 0;
	}

	DBGLOG(REQ, LOUD, "fgSapRpsEnable %u\n",
		prAdapter->rWifiVar.fgSapRpsEnable);
	return WLAN_STATUS_SUCCESS;
#else
	DBGLOG(REQ, WARN, "not support GETSTAINFO\n");
	return -EOPNOTSUPP;
#endif
}

int testmode_set_rps_param(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
#if CFG_SAP_RPS_SUPPORT
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct ADAPTER *prAdapter = NULL;
	uint32_t u4SetSapIps;
	uint8_t  ucSapPhase;
	uint32_t u4SapThreshldTime;
	uint8_t  ucSapStatus;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (!prGlueInfo)
		return -EOPNOTSUPP;

	DBGLOG(REQ, WARN, "SET_AP_RPS cmd check ok\n");

	kalkStrtou32(apcArgv[1], 0, &u4SetSapIps);
	kalkStrtou8(apcArgv[2], 0, &ucSapPhase);
	kalkStrtou32(apcArgv[3], 0, &u4SapThreshldTime);
	kalkStrtou8(apcArgv[4], 0, &ucSapStatus);

	prAdapter = prGlueInfo->prAdapter;
	if (!prAdapter)
		return -EOPNOTSUPP;

	prAdapter->rWifiVar.u4RpsInpktThresh = u4SetSapIps;
	prAdapter->rWifiVar.ucSapRpsPhase = ucSapPhase;
	prAdapter->rWifiVar.u4RpsTriggerTime = u4SapThreshldTime* 1000;
	prAdapter->rWifiVar.ucSapRpsStatus = ucSapStatus;

	DBGLOG(REQ, LOUD, "fgSapRpsEnable %u\n",
		prAdapter->rWifiVar.fgSapRpsEnable);
	return WLAN_STATUS_SUCCESS;
#else
	DBGLOG(REQ, WARN, "not support GETSTAINFO\n");
	return -EOPNOTSUPP;
#endif
}

#if CFG_SUPPORT_MANIPULATE_TID
int testmode_manipulate_tid(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint32_t u4SetInfoLen = 0;
	struct GLUE_INFO *prGlueInfo;
	struct PARAM_MANIUPLATE_TID param = { 0 };

	WIPHY_PRIV(wiphy, prGlueInfo);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc != 4) {
		DBGLOG(REQ, ERROR,
			"Error input parameters(%d):%s\n", i4Argc, pcCommand);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (kalkStrtou8(apcArgv[1], 0, &param.ucMode))
		DBGLOG(REQ, LOUD, "Mode parse %s error\n", apcArgv[1]);

	if (kalkStrtou8(apcArgv[3], 0, &param.ucTid))
		DBGLOG(REQ, LOUD, "Tid parse %s error\n", apcArgv[3]);

	rStatus = kalIoctl(prGlueInfo, wlanoidManipulateTid,
			&param, sizeof(struct PARAM_MANIUPLATE_TID),
			&u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR,
		       "SET_TID fail 0x%x\n", rStatus);
	else
		DBGLOG(INIT, TRACE,
		       "SET_TID successed\n");

	return rStatus;
}
#endif /* CFG_SUPPORT_MANIPULATE_TID */

#if CFG_SUPPORT_NCHO
int testmode_set_ncho_roam_trigger(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam trigger cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamTrigger,
				   &i4Param, sizeof(int32_t), &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "NCHO set roam trigger fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "NCHO set roam trigger successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_trigger(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamTrigger,
			   &cmdV1Header, sizeof(cmdV1Header), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamTrigger fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &i4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_NOT_INDICATING;
	}

	i4Param = RCPI_TO_dBm(i4Param);		/* RCPI to DB */
	i4BytesWritten = snprintf(buf, 512,
		CMD_NCHO_ROAM_TRIGGER_GET" %d", i4Param);
	if (i4BytesWritten < 0) {
		DBGLOG(INIT, ERROR, "Error writing to buffer\n");
		return WLAN_STATUS_FAILURE;
	} else if (i4BytesWritten >= 512) {
		DBGLOG(INIT, ERROR, "Buffer too small\n");
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	DBGLOG(INIT, INFO, "NCHO query RoamTrigger is [%s][%s]\n",
		cmdV1Header.buffer, pcCommand);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_delta(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR,
			       "NCHO parse u4Param error %d\n", i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam delta cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamDelta,
				   &i4Param, sizeof(int32_t), &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "NCHO set roam delta fail 0x%x\n", rStatus);
		else
			DBGLOG(INIT, TRACE, "NCHO set roam delta successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_delta(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamDelta,
			   &i4Param, sizeof(int32_t), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamDelta fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, 512,
		CMD_NCHO_ROAM_DELTA_GET" %d", i4Param);
	if (i4BytesWritten < 0) {
		DBGLOG(INIT, ERROR, "Error writing to buffer\n");
		return WLAN_STATUS_FAILURE;
	} else if (i4BytesWritten >= 512) {
		DBGLOG(INIT, ERROR, "Buffer too small\n");
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
	DBGLOG(REQ, TRACE, "NCHO query ok and ret is [%d][%s]\n",
		i4Param, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_scn_period(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam period cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamScnPeriod,
				   &u4Param, sizeof(uint32_t), &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "NCHO set roam period fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE, "NCHO set roam period successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_scn_period(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnPeriod,
			   &cmdV1Header, sizeof(cmdV1Header), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamTrigger fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &i4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_NOT_INDICATING;
	}

	i4BytesWritten = snprintf(buf, 512,
		CMD_NCHO_ROAM_SCAN_PERIOD_GET" %d", i4Param);
	if (i4BytesWritten < 0) {
		DBGLOG(INIT, ERROR, "Error writing to buffer\n");
		return WLAN_STATUS_FAILURE;
	} else if (i4BytesWritten >= 512) {
		DBGLOG(INIT, ERROR,	"Buffer too small\n");
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
	DBGLOG(INIT, INFO, "NCHO query Roam Period is [%s][%s]\n",
		cmdV1Header.buffer, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_scn_chnl_impl(
	struct wiphy *wiphy,
	char *pcCommand,
	int i4TotalLen,
	uint8_t changeMode)
{
	uint32_t u4ChnlInfo = 0;
	uint8_t i = 1;
	uint8_t t = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CFG_NCHO_SCAN_CHNL *prExistingRoamScnChnl;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	prExistingRoamScnChnl =
		&prGlueInfo->prAdapter->rNchoInfo.rAddRoamScnChnl;

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, cmd is %s\n", i4Argc,
		       apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		rRoamScnChnl.ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, INFO, "NCHO ChannelListNum is %d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "NCHO param mismatch %d\n",
			       u4ChnlInfo);
			return WLAN_STATUS_INVALID_DATA;
		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4ChnlInfo);
			if (i4Ret) {
				while (i != 2) {
					rRoamScnChnl.arChnlInfoList[i]
					.ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR,
				       "NCHO parse chnl num error %d\n", i4Ret);
				return -1;
			}
			if (u4ChnlInfo != 0) {
				DBGLOG(INIT, TRACE,
				       "NCHO t = %d, channel value=%d\n",
				       t, u4ChnlInfo);
				if ((u4ChnlInfo >= 1) && (u4ChnlInfo <= 14))
					rRoamScnChnl.arChnlInfoList[t].eBand =
								BAND_2G4;
				else
					rRoamScnChnl.arChnlInfoList[t].eBand =
								BAND_5G;

				rRoamScnChnl.arChnlInfoList[t].ucChannelNum =
								u4ChnlInfo;
				t++;
			}
		}

		/* Add existing roam settings */
		for (i = 0; t < MAXIMUM_OPERATION_CHANNEL_LIST &&
			i < prExistingRoamScnChnl->ucChannelListNum; i++, t++) {
			rRoamScnChnl.arChnlInfoList[t].eBand =
				prExistingRoamScnChnl->arChnlInfoList[i].eBand;
			rRoamScnChnl.arChnlInfoList[t].ucChannelNum =
				prExistingRoamScnChnl->arChnlInfoList[
					i].ucChannelNum;
			rRoamScnChnl.arChnlInfoList[t].u2PriChnlFreq =
				prExistingRoamScnChnl->arChnlInfoList[
					i].u2PriChnlFreq;
			rRoamScnChnl.ucChannelListNum++;
		}

		/* Add existing roam settings */
		for (i = 0; t < MAXIMUM_OPERATION_CHANNEL_LIST &&
			i < prExistingRoamScnChnl->ucChannelListNum; i++, t++) {
			rRoamScnChnl.arChnlInfoList[t].eBand =
				prExistingRoamScnChnl->arChnlInfoList[i].eBand;
			rRoamScnChnl.arChnlInfoList[t].ucChannelNum =
				prExistingRoamScnChnl->arChnlInfoList[
					i].ucChannelNum;
			rRoamScnChnl.arChnlInfoList[t].u2PriChnlFreq =
				prExistingRoamScnChnl->arChnlInfoList[
					i].u2PriChnlFreq;
			rRoamScnChnl.ucChannelListNum++;
		}

		DBGLOG(INIT, TRACE, "NCHO %s roam scan channel cmd\n",
			changeMode ? "set" : "add");
		if (changeMode)
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetNchoRoamScnChnl, &rRoamScnChnl,
				sizeof(struct CFG_NCHO_SCAN_CHNL),
				&u4SetInfoLen);
		else
			rStatus = kalIoctl(prGlueInfo,
				wlanoidAddNchoRoamScnChnl, &rRoamScnChnl,
				sizeof(struct CFG_NCHO_SCAN_CHNL),
				&u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "NCHO set roam scan channel fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "NCHO set roam scan channel successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_set_ncho_roam_scn_chnl_set(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	return testmode_set_ncho_roam_scn_chnl_impl(
		wiphy,
		pcCommand,
		i4TotalLen,
		1);
}

int testmode_set_ncho_roam_scn_chnl_add(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	return testmode_set_ncho_roam_scn_chnl_impl(
		wiphy,
		pcCommand,
		i4TotalLen,
		0);
}

int testmode_get_ncho_roam_scn_chnl(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t i = 0;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = -1;
	int32_t i4Argc = 0;
	uint32_t u4ChnlInfo = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnChnl,
			   &rRoamScnChnl, sizeof(struct CFG_NCHO_SCAN_CHNL),
			   &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamScnChnl fail 0x%x\n", rStatus);
		return rStatus;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n",
	       rRoamScnChnl.ucChannelListNum);
	u4ChnlInfo = rRoamScnChnl.ucChannelListNum;
	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
				   512 - i4BytesWritten,
				   CMD_NCHO_ROAM_SCAN_CHANNELS_GET" %u",
				   u4ChnlInfo);
	for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
		u4ChnlInfo =
			rRoamScnChnl.arChnlInfoList[i].ucChannelNum;
		i4BytesWritten += snprintf(buf + i4BytesWritten,
				       512 - i4BytesWritten,
				       " %u", u4ChnlInfo);
	}

	DBGLOG(REQ, INFO, "NCHO get scn chl list num is [%d][%s]\n",
	       rRoamScnChnl.ucChannelListNum, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_scn_ctrl(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam scan control cmd %d\n",
		       u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamScnCtrl,
				   &u4Param, sizeof(uint32_t), &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "NCHO set roam scan control fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "NCHO set roam scan control successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_get_ncho_roam_scn_ctrl(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnCtrl,
			   &u4Param, sizeof(uint32_t), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamScnCtrl fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, 512,
		CMD_NCHO_ROAM_SCAN_CONTROL_GET" %u", u4Param);
	if (i4BytesWritten < 0) {
		DBGLOG(REQ, ERROR, "Error writing to buffer\n");
		return WLAN_STATUS_FAILURE;
	} else if (i4BytesWritten >= 512) {
		DBGLOG(REQ, ERROR, "Buffer too small\n");
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
	DBGLOG(REQ, INFO, "NCHO query ok and ret is [%u][%s]\n",
		u4Param, buf);
	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_mode_impl(
	struct wiphy *wiphy,
	char *pcCommand,
	int i4TotalLen,
	uint8_t ucBssIndex)

{
	uint32_t u4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc,
		       apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set enable cmd %d\n",
			       u4Param);
			rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidSetNchoEnable, &u4Param,
				sizeof(uint32_t), &u4SetInfoLen,
				ucBssIndex);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, ERROR,
				       "NCHO set enable fail 0x%x\n", rStatus);
			else
				DBGLOG(INIT, TRACE,
				       "NCHO set enable successed\n");
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

int testmode_set_ncho_mode(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	return testmode_set_ncho_mode_impl(wiphy,
		pcCommand,
		i4TotalLen,
		ucBssIndex);
}

int testmode_get_ncho_mode(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];
	WIPHY_PRIV(wiphy, prGlueInfo);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}

	/*<2> Query NCHOEnable Satus*/
	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoEnable,
			   &cmdV1Header, sizeof(cmdV1Header), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoEnable fail 0x%x\n",
		       rStatus);
		return rStatus;
	}

	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_INVALID_DATA;
	}

	i4BytesWritten = snprintf(buf, 512, CMD_NCHO_MODE_GET" %u", u4Param);
	if (i4BytesWritten < 0) {
		DBGLOG(INIT, ERROR, "Error writing to buffer\n");
		return WLAN_STATUS_FAILURE;
	} else if (i4BytesWritten >= 512) {
		DBGLOG(INIT, ERROR,	"Buffer too small\n");
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
	DBGLOG(REQ, INFO, "NCHO query ok and ret is [%u][%s]\n",
		u4Param, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_scn_freq_impl(
	struct wiphy *wiphy,
	char *pcCommand,
	int i4TotalLen,
	uint8_t changeMode)
{
	uint32_t u4ChnlInfo = 0, u4FreqInfo = 0;
	uint8_t i = 1;
	uint8_t t = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, cmd is %s\n", i4Argc,
		       apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		rRoamScnChnl.ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, INFO, "NCHO ChannelListNum is %d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "NCHO param mismatch %d\n",
			       u4ChnlInfo);
			return WLAN_STATUS_INVALID_DATA;
		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4FreqInfo);
			if (i4Ret) {
				while (i != 2) {
					rRoamScnChnl.arChnlInfoList[i]
					.ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR,
				       "NCHO parse chnl num error %d\n", i4Ret);
				return -1;
			}
			if (u4FreqInfo != 0) {
				DBGLOG(INIT, TRACE,
				       "NCHO t = %d, Freq=%d\n", t, u4FreqInfo);
				/* Input freq unit should be kHz */
				u4ChnlInfo = nicFreq2ChannelNum(
					u4FreqInfo * 1000);
				if (u4FreqInfo >= 2412 && u4FreqInfo <= 2484)
					rRoamScnChnl.arChnlInfoList[t].eBand =
						BAND_2G4;
				else if (u4FreqInfo >= 5180 &&
					u4FreqInfo <= 5900)
					rRoamScnChnl.arChnlInfoList[t].eBand =
						BAND_5G;
#if (CFG_SUPPORT_WIFI_6G == 1)
				else if (u4FreqInfo >= 5955 &&
					u4FreqInfo <= 7115)
					rRoamScnChnl.arChnlInfoList[t].eBand =
						BAND_6G;
#endif

				rRoamScnChnl.arChnlInfoList[t].ucChannelNum =
								u4ChnlInfo;
				rRoamScnChnl.arChnlInfoList[t].u2PriChnlFreq =
								u4FreqInfo;
				t++;
			}
		}

		DBGLOG(INIT, TRACE, "NCHO %s roam scan channel cmd\n",
			changeMode ? "set" : "add");
		if (changeMode)
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetNchoRoamScnChnl, &rRoamScnChnl,
				sizeof(struct CFG_NCHO_SCAN_CHNL),
				&u4SetInfoLen);
		else
			rStatus = kalIoctl(prGlueInfo,
				wlanoidAddNchoRoamScnChnl, &rRoamScnChnl,
				sizeof(struct CFG_NCHO_SCAN_CHNL),
				&u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "NCHO set roam scan channel fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "NCHO set roam scan channel successed\n");
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_set_ncho_roam_scn_freq_set(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	return testmode_set_ncho_roam_scn_freq_impl(
		wiphy,
		pcCommand,
		i4TotalLen,
		1);
}

int testmode_set_ncho_roam_scn_freq_add(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	return testmode_set_ncho_roam_scn_freq_impl(
		wiphy,
		pcCommand,
		i4TotalLen,
		0);
}

int testmode_get_ncho_roam_scn_freq(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t i = 0;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = -1;
	int32_t i4Argc = 0;
	uint32_t u4ChnlInfo = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnChnl,
			   &rRoamScnChnl, sizeof(struct CFG_NCHO_SCAN_CHNL),
			   &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamScnChnl fail 0x%x\n", rStatus);
		return rStatus;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n",
	       rRoamScnChnl.ucChannelListNum);
	u4ChnlInfo = rRoamScnChnl.ucChannelListNum;
	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
				   512 - i4BytesWritten,
				   "%s %u",
				   apcArgv[0], u4ChnlInfo);
	for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
		u4ChnlInfo =
			rRoamScnChnl.arChnlInfoList[i].u2PriChnlFreq;
		i4BytesWritten += snprintf(buf + i4BytesWritten,
				       512 - i4BytesWritten,
				       " %u", u4ChnlInfo);
	}

	DBGLOG(REQ, INFO, "NCHO get scn freq list num is [%d][%s]\n",
	       rRoamScnChnl.ucChannelListNum, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_get_cu_impl(
	struct wiphy *wiphy,
	char *pcCommand,
	int i4TotalLen,
	uint8_t ucBssIndex)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct AIS_FSM_INFO *prAisFsmInfo;
	struct BSS_INFO *prBssInfo;
	struct BSS_DESC *prBssDesc;
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct STA_RECORD *prStaRec;
	struct MLD_STA_RECORD *prMldStaRec;
	uint8_t i = 0, linkID[16] = {0};
	int32_t linkCU[16] = {0};
#endif
	int32_t i4RetValue = -1;
	char buf[64] = {0};

	WIPHY_PRIV(wiphy, prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;

	prBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prBssDesc = aisGetTargetBssDesc(prAdapter, ucBssIndex);

	/* Disconnected case */
	if (prBssInfo->eConnectionState != MEDIA_STATE_CONNECTED ||
	    !prBssDesc) {
		snprintf(buf, sizeof(buf), "%d", i4RetValue);
		DBGLOG(REQ, TRACE, "Disconnected: %s\n", buf);

		goto reply_command;
	}

	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

	/* Single link or leagcy case */
	if (aisGetLinkNum(prAisFsmInfo) == 1) {
		if (prBssDesc->fgExistBssLoadIE)
			i4RetValue = (int32_t) prBssDesc->ucChnlUtilization;

		snprintf(buf, sizeof(buf), "%d", i4RetValue);
		DBGLOG(REQ, TRACE, "Current CU: %s\n", buf);

		goto reply_command;
	}

	/* MLO case */
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	for (i = 0; i < MLD_LINK_MAX; i++) {
		prBssDesc = aisGetLinkBssDesc(prAisFsmInfo, i);
		prStaRec = aisGetLinkStaRec(prAisFsmInfo, i);
		prMldStaRec = mldStarecGetByStarec(prAdapter, prStaRec);

		if (!prBssDesc || !prStaRec ||
		    !prMldStaRec || prStaRec->ucLinkIndex >= MAX_NUM_MLO_LINKS)
			continue;

		/* Connected link from this link ID */
		linkID[prStaRec->ucLinkIndex] = 1;

		if ((prMldStaRec->u4ActiveStaBitmap & BIT(prStaRec->ucIndex)) &&
		    prBssDesc->fgExistBssLoadIE)
			linkCU[prStaRec->ucLinkIndex] =
				(int32_t) prBssDesc->ucChnlUtilization;
		else
			linkCU[prStaRec->ucLinkIndex] = -1;
	}

	/* Sort by Link ID */
	for (i = 0; i < 16; i++) {
		if (linkID[i] && strlen(buf) == 0)
			snprintf(buf + strlen(buf),
			    sizeof(buf) - strlen(buf), "%d:%d", i, linkCU[i]);
		else if (linkID[i] && strlen(buf) > 0)
			snprintf(buf + strlen(buf),
			    sizeof(buf) - strlen(buf), " %d:%d", i, linkCU[i]);
	}

	DBGLOG(REQ, TRACE, "Current MLO CU: %s\n", buf);
#endif

reply_command:
	return mtk_cfg80211_process_str_cmd_reply(wiphy, buf, strlen(buf) + 1);
}

int testmode_get_cu(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	return testmode_get_cu_impl(wiphy,
		pcCommand,
		i4TotalLen,
		ucBssIndex);
}

int testmode_get_ncho_roam_band(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamBand,
			   &i4Param, sizeof(int32_t), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamDelta fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, 512,
		CMD_NCHO_ROAM_BAND_GET" %d", i4Param);
	if (i4BytesWritten < 0) {
		DBGLOG(REQ, ERROR, "Error writing to buffer\n");
		return WLAN_STATUS_FAILURE;
	} else if (i4BytesWritten >= 512) {
		DBGLOG(REQ, ERROR, "Buffer too small\n");
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}
	DBGLOG(REQ, TRACE, "NCHO query ok and ret is [%d][%s]\n",
		i4Param, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_ncho_roam_band(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint32_t u4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;

	WIPHY_PRIV(wiphy, prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc,
		       apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set roam band:%d\n", u4Param);

			if (u4Param >= NCHO_ROAM_BAND_MAX)
				return WLAN_STATUS_INVALID_DATA;

			rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamBand,
				&u4Param, sizeof(uint32_t), &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, ERROR,
				       "NCHO set roam band fail 0x%x\n",
				       rStatus);
			else
				DBGLOG(INIT, TRACE,
				       "NCHO set roam band successed\n");
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set roam band failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

int testmode_reassoc_impl(
	struct wiphy *wiphy,
	char *pcCommand,
	int i4TotalLen,
	uint8_t ucBssIndex,
	uint8_t reassocWithChl,
	uint8_t isLegacyCmd)
{
	uint32_t u4InputInfo = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CONNECT rNewSsid;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
	uint8_t bssid[MAC_ADDR_LEN];
	uint8_t ucSSIDLen;
	uint8_t aucSSID[ELEM_MAX_LEN_SSID] = {0};
	uint16_t u2LinkIdBitmap = 0xFFFF;

	DBGLOG(INIT, INFO, "command is %s\n", pcCommand);
	DBGLOG(INIT, INFO, "reassoc with %s (%s mode)\n",
		reassocWithChl ? "channel" : "frequency",
		isLegacyCmd ? "legacy" : "NCHO");
	WIPHY_PRIV(wiphy, prGlueInfo);
	prConnSettings = aisGetConnSettings(prGlueInfo->prAdapter, ucBssIndex);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (!(isLegacyCmd ^ prGlueInfo->prAdapter->rNchoInfo.fgNCHOEnabled)) {
		DBGLOG(REQ, ERROR,
			"Command(%s) doesn't match current mode(%s)\n",
			isLegacyCmd ? "-legacy" : "-NCHO",
			prGlueInfo->prAdapter->rNchoInfo.fgNCHOEnabled ?
			"-NCHO" : "-legacy");
		return WLAN_STATUS_INVALID_DATA;
	}

	if (i4Argc >= 3) {
		DBGLOG(REQ, TRACE, "argc is %i, cmd is %s, %s\n", i4Argc,
		       apcArgv[1], apcArgv[2]);
		i4Ret = kalkStrtou32(apcArgv[2], 0, &u4InputInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n", i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}
		/* copy ssd to local instead of assigning
		 * prConnSettings->aucSSID to rNewSsid.pucSsid because
		 * wlanoidSetConnect will copy ssid from rNewSsid again
		 */
		COPY_SSID(aucSSID, ucSSIDLen,
			prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

		wlanHwAddrToBin(apcArgv[1], bssid);
		if (i4Argc >= 4)
			i4Ret = kalkStrtou16(apcArgv[3], 0, &u2LinkIdBitmap);
		if (i4Ret) {
			DBGLOG(REQ, ERROR,
				"parse u2LinkIdBitmap error %d\n", i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		kalMemZero(&rNewSsid, sizeof(rNewSsid));
		rNewSsid.u4CenterFreq = reassocWithChl ?
			(nicChannelNum2Freq(u4InputInfo, BAND_NULL) / 1000) :
			u4InputInfo;
		rNewSsid.pucBssid = NULL;
		rNewSsid.pucBssidHint = bssid;
		rNewSsid.pucSsid = aucSSID;
		rNewSsid.u4SsidLen = ucSSIDLen;
		rNewSsid.ucBssIdx = ucBssIndex;
		rNewSsid.fgTestMode = TRUE;
		rNewSsid.u2LinkIdBitmap = u2LinkIdBitmap;

		DBGLOG(INIT, INFO,
		       "Reassoc ssid=%s(%d) bssid=" MACSTR
		       " freq=%d LinkIdBitmap=%d\n",
		       rNewSsid.pucSsid, rNewSsid.u4SsidLen,
		       MAC2STR(bssid), rNewSsid.u4CenterFreq, u2LinkIdBitmap);

		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetConnect,
			   (void *)&rNewSsid, sizeof(struct PARAM_CONNECT),
			   &u4SetInfoLen, ucBssIndex);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "reassoc fail 0x%x\n", rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "reassoc successed\n");
	} else {
		DBGLOG(REQ, ERROR, "reassoc failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

int testmode_reassoc0(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	return testmode_reassoc_impl(wiphy,
		pcCommand,
		i4TotalLen,
		ucBssIndex,
		0, 0);
}

int testmode_reassoc1(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	return testmode_reassoc_impl(wiphy,
		pcCommand,
		i4TotalLen,
		ucBssIndex,
		0, 1);
}

int testmode_reassoc2(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	return testmode_reassoc_impl(wiphy,
		pcCommand,
		i4TotalLen,
		ucBssIndex,
		1, 0);
}

int testmode_reassoc3(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	return testmode_reassoc_impl(wiphy,
		pcCommand,
		i4TotalLen,
		ucBssIndex,
		1, 1);
}

int testmode_add_roam_scn_chnl(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint32_t u4ChnlInfo = 0;
	uint8_t i = 1;
	uint8_t t = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CFG_NCHO_SCAN_CHNL *prRoamScnChnl;
	struct CFG_NCHO_SCAN_CHNL *prExistingRoamScnChnl;
	struct GLUE_INFO *prGlueInfo = NULL;

	WIPHY_PRIV(wiphy, prGlueInfo);

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	prExistingRoamScnChnl = &prGlueInfo->prAdapter->rAddRoamScnChnl;

	prRoamScnChnl = kalMemAlloc(sizeof(struct CFG_NCHO_SCAN_CHNL), VIR_MEM_TYPE);
	if (prRoamScnChnl == NULL) {
		DBGLOG(REQ, ERROR, "alloc roaming scan channel fail\n");
		return WLAN_STATUS_RESOURCES;
	}
	kalMemZero(prRoamScnChnl, sizeof(struct CFG_NCHO_SCAN_CHNL));

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc is %i, cmd is %s\n", i4Argc,
		       apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
			       i4Ret);
			rStatus = WLAN_STATUS_INVALID_DATA;
			goto label_exit;
		}

		prRoamScnChnl->ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, INFO, "ChannelListNum is %d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "param mismatch %d\n",
			       u4ChnlInfo);
			rStatus = WLAN_STATUS_INVALID_DATA;
			goto label_exit;

		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4ChnlInfo);
			if (i4Ret) {
				while (i != 2) {
					prRoamScnChnl->arChnlInfoList[i]
					.ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR,
				       "parse chnl num error %d\n", i4Ret);
				rStatus = WLAN_STATUS_FAILURE;
				goto label_exit;
			}
			if (u4ChnlInfo != 0) {
				DBGLOG(INIT, TRACE,
				       "t = %d, channel value=%d\n",
				       t, u4ChnlInfo);
				if ((u4ChnlInfo >= 1) && (u4ChnlInfo <= 14))
					prRoamScnChnl->arChnlInfoList[t].eBand =
								BAND_2G4;
				else
					prRoamScnChnl->arChnlInfoList[t].eBand =
								BAND_5G;

				prRoamScnChnl->arChnlInfoList[t].ucChannelNum =
								u4ChnlInfo;
				t++;
			}
		}

		/* Add existing roam settings */
		for (i = 0; t < MAXIMUM_OPERATION_CHANNEL_LIST &&
			i < prExistingRoamScnChnl->ucChannelListNum; i++, t++) {
			prRoamScnChnl->arChnlInfoList[t].eBand =
				prExistingRoamScnChnl->arChnlInfoList[i].eBand;
			prRoamScnChnl->arChnlInfoList[t].ucChannelNum =
				prExistingRoamScnChnl->arChnlInfoList[
					i].ucChannelNum;
			prRoamScnChnl->arChnlInfoList[t].u2PriChnlFreq =
				prExistingRoamScnChnl->arChnlInfoList[
					i].u2PriChnlFreq;
			prRoamScnChnl->ucChannelListNum++;
		}

		rStatus = kalIoctl(prGlueInfo, wlanoidAddRoamScnChnl,
			prRoamScnChnl,
			sizeof(struct CFG_NCHO_SCAN_CHNL),
			&u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "add roam scan channel fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "add roam scan channel successed\n");
		goto label_exit;
	} else {
		DBGLOG(REQ, ERROR, "add failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
		goto label_exit;
	}

label_exit:
	kalMemFree(prRoamScnChnl,
		sizeof(struct CFG_NCHO_SCAN_CHNL),
		VIR_MEM_TYPE);
	return rStatus;
}

int testmode_get_roam_scn_chnl_impl(
	struct wiphy *wiphy,
	char *pcCommand,
	int i4TotalLen,
	uint8_t ucBssIndex)
{
	uint8_t i = 0;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = -1;
	int32_t i4Argc = 0;
	uint32_t u4ChnlInfo = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "wrong input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctlByBssIdx(prGlueInfo,
		wlanoidQueryRoamScnChnl,
		&rRoamScnChnl,
		sizeof(struct CFG_NCHO_SCAN_CHNL),
		&u4BufLen,
		ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanoidQueryRoamScnChnl fail 0x%x\n", rStatus);
		return rStatus;
	}

	DBGLOG(REQ, TRACE, "query ok and ret is %d\n",
	       rRoamScnChnl.ucChannelListNum);
	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
				   512 - i4BytesWritten,
				   "GETROAMSCANCHANNELS_LEGACY %u",
				   rRoamScnChnl.ucChannelListNum);
	for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
		u4ChnlInfo = rRoamScnChnl.arChnlInfoList[i].ucChannelNum;
		i4BytesWritten += snprintf(buf + i4BytesWritten,
				       512 - i4BytesWritten,
				       " %u", u4ChnlInfo);
	}

	DBGLOG(REQ, INFO, "Get roam scn chl list num is [%d][%s]\n",
	       rRoamScnChnl.ucChannelListNum, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_get_roam_scn_chnl(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	return testmode_get_roam_scn_chnl_impl(
		wiphy,
		pcCommand,
		i4TotalLen,
		ucBssIndex);
}

int testmode_get_roam_scn_freq(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint8_t i = 0;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = -1;
	int32_t i4Argc = 0;
	uint32_t u4ChnlInfo = 0;
	enum ENUM_BAND eBand;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "wrong input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryRoamScnChnl,
			   &rRoamScnChnl, sizeof(struct CFG_NCHO_SCAN_CHNL),
			   &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanoidQueryRoamScnChnl fail 0x%x\n", rStatus);
		return rStatus;
	}

	DBGLOG(REQ, TRACE, "query ok and ret is %d\n",
	       rRoamScnChnl.ucChannelListNum);
	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
				   512 - i4BytesWritten,
				   "GETROAMSCANFREQUENCIES_LEGACY %u",
				   rRoamScnChnl.ucChannelListNum);
	for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
		u4ChnlInfo = rRoamScnChnl.arChnlInfoList[i].ucChannelNum;
		eBand = rRoamScnChnl.arChnlInfoList[i].eBand;
		i4BytesWritten += snprintf(buf + i4BytesWritten,
				       512 - i4BytesWritten,
				       " %u",
				       KHZ_TO_MHZ(nicChannelNum2Freq(
				       u4ChnlInfo, eBand)));
	}

	DBGLOG(REQ, INFO, "Get roam scn freq list num is [%d][%s]\n",
	       rRoamScnChnl.ucChannelListNum, buf);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_add_roam_scn_freq(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint32_t u4ChnlInfo = 0, u4FreqInfo = 0;
	uint8_t i = 1;
	uint8_t t = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CFG_NCHO_SCAN_CHNL *prRoamScnChnl;
	struct CFG_NCHO_SCAN_CHNL *prExistingRoamScnChnl;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	prExistingRoamScnChnl = &prGlueInfo->prAdapter->rAddRoamScnChnl;

	prRoamScnChnl = kalMemAlloc(
		sizeof(struct CFG_NCHO_SCAN_CHNL),
		VIR_MEM_TYPE);
	if (prRoamScnChnl == NULL) {
		DBGLOG(REQ, ERROR, "alloc roaming scan frequency fail\n");
		return WLAN_STATUS_RESOURCES;
	}
	kalMemZero(prRoamScnChnl,
		sizeof(struct CFG_NCHO_SCAN_CHNL));

	if (i4Argc >= 2) {
		DBGLOG(REQ, INFO, "argc[%i]\n", i4Argc);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
			       i4Ret);
			rStatus = WLAN_STATUS_INVALID_DATA;
			goto label_exit;
		}

		prRoamScnChnl->ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, INFO, "Frequency count:%d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "param mismatch %d\n", u4ChnlInfo);
			rStatus = WLAN_STATUS_INVALID_DATA;
			goto label_exit;

		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4FreqInfo);
			if (i4Ret) {
				while (i != 2) {
					prRoamScnChnl->arChnlInfoList[i]
					.ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR,
				       "parse chnl num error %d\n", i4Ret);
				rStatus = WLAN_STATUS_FAILURE;
				goto label_exit;
			}
			if (u4FreqInfo != 0) {
				DBGLOG(INIT, TRACE,
				       "[%d] freq=%d\n", t, u4FreqInfo);
				/* Input freq unit should be kHz */
				u4ChnlInfo = nicFreq2ChannelNum(
					u4FreqInfo * 1000);
				if (u4FreqInfo >= 2412 && u4FreqInfo <= 2484)
					prRoamScnChnl->arChnlInfoList[t].eBand =
						BAND_2G4;
				else if (u4FreqInfo >= 5180 &&
					u4FreqInfo <= 5900)
					prRoamScnChnl->arChnlInfoList[t].eBand =
						BAND_5G;
#if (CFG_SUPPORT_WIFI_6G == 1)
				else if (u4FreqInfo >= 5955 &&
					u4FreqInfo <= 7115)
					prRoamScnChnl->arChnlInfoList[t].eBand =
						BAND_6G;
#endif

				prRoamScnChnl->arChnlInfoList[t].ucChannelNum =
								u4ChnlInfo;
				prRoamScnChnl->arChnlInfoList[t].u2PriChnlFreq =
								u4FreqInfo;
				t++;
			}

		}

		/* Add existing roam settings */
		for (i = 0; t < MAXIMUM_OPERATION_CHANNEL_LIST &&
			i < prExistingRoamScnChnl->ucChannelListNum; i++, t++) {
			prRoamScnChnl->arChnlInfoList[t].eBand =
				prExistingRoamScnChnl->arChnlInfoList[i].eBand;
			prRoamScnChnl->arChnlInfoList[t].ucChannelNum =
				prExistingRoamScnChnl->arChnlInfoList[
					i].ucChannelNum;
			prRoamScnChnl->arChnlInfoList[t].u2PriChnlFreq =
				prExistingRoamScnChnl->arChnlInfoList[
					i].u2PriChnlFreq;
			prRoamScnChnl->ucChannelListNum++;
		}

		rStatus = kalIoctl(prGlueInfo, wlanoidAddRoamScnChnl,
			prRoamScnChnl, sizeof(struct CFG_NCHO_SCAN_CHNL),
			&u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR,
			       "add roam scan channel fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "add roam scan channel successed\n");
		goto label_exit;
	} else {
		DBGLOG(REQ, ERROR, "add failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
		goto label_exit;
	}

label_exit:
	kalMemFree(prRoamScnChnl,
		sizeof(struct CFG_NCHO_SCAN_CHNL),
		VIR_MEM_TYPE);
	return rStatus;
}

int testmode_get_roam_trigger(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);

	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "error input parameter %d\n", i4Argc);
		return rStatus;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryRoamTrigger,
			   &cmdV1Header, sizeof(cmdV1Header), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanoidQueryRoamTrigger fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &i4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "parse u4Param error %d!\n",
		       i4BytesWritten);
		return WLAN_STATUS_NOT_INDICATING;
	}

	i4Param = RCPI_TO_dBm(i4Param);		/* RCPI to DB */
	i4BytesWritten = snprintf(buf, 512,
		"GETROAMTRIGGER_LEGACY %d", i4Param);
	DBGLOG(INIT, INFO, "query RoamTrigger is [%s][%s]\n",
		cmdV1Header.buffer, pcCommand);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_roam_trigger(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return WLAN_STATUS_INVALID_DATA;
		}

		DBGLOG(INIT, TRACE, "set roam trigger cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetRoamTrigger,
				   &i4Param, sizeof(int32_t), &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "set roam trigger fail 0x%x\n",
			       rStatus);
		else
			DBGLOG(INIT, TRACE,
			       "set roam trigger successed\n");
	} else {
		DBGLOG(REQ, ERROR, "set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}
	return rStatus;
}

int testmode_set_wtc_mode(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct WIFI_VAR *prWifiVar = NULL;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;
	prWifiVar = &prAdapter->rWifiVar;
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc <= 7) {
		/* WTC mode */
		if (i4Argc >= 2 && kalkStrtou8(
			apcArgv[1], 0, &prAdapter->rWtcModeInfo.ucWtcMode)) {
			DBGLOG(REQ, LOUD,
				"WTC mode parse %s err\n", apcArgv[1]);
			return WLAN_STATUS_INVALID_DATA;
		}

		/* scan mode */
		if (i4Argc >= 3 && kalkStrtou8(
			apcArgv[2], 0, &prAdapter->rWtcModeInfo.ucScanMode)) {
			DBGLOG(REQ, LOUD,
				"scan mode parse %s err\n", apcArgv[2]);
			return WLAN_STATUS_INVALID_DATA;
		} else {
			prAdapter->rWtcModeInfo.ucScanMode = prWifiVar->ucScanMode;
		}

		/* rssi threshold */
		if (i4Argc >= 4 && kalStrtoint(apcArgv[3], 0,
			&prAdapter->rWtcModeInfo.cRssiThreshold)) {
			DBGLOG(REQ, LOUD,
				"rssi threshold %s err\n", apcArgv[3]);
			return WLAN_STATUS_INVALID_DATA;
		} else {
			prAdapter->rWtcModeInfo.cRssiThreshold = prWifiVar->cRssiThreshold;
		}

		/* candidate 2.4G rssi threshold */
		if (i4Argc >= 5 && kalStrtoint(apcArgv[4], 0,
			&prAdapter->rWtcModeInfo.cRssiThreshold_24G)) {
			DBGLOG(REQ, LOUD,
				"2.4G rssi threshold %s err\n", apcArgv[4]);
			return WLAN_STATUS_INVALID_DATA;
		} else {
			prAdapter->rWtcModeInfo.cRssiThreshold_24G = prWifiVar->cRssiThreshold_24G;
		}

		/* candidate 5G rssi threshold */
		if (i4Argc >= 6 && kalStrtoint(apcArgv[5], 0,
			&prAdapter->rWtcModeInfo.cRssiThreshold_5G)) {
			DBGLOG(REQ, LOUD,
				"2.4G rssi threshold %s err\n", apcArgv[5]);
			return WLAN_STATUS_INVALID_DATA;
		} else {
			prAdapter->rWtcModeInfo.cRssiThreshold_5G = prWifiVar->cRssiThreshold_5G;
		}

#if (CFG_SUPPORT_WIFI_6G == 1)
		/* candidate 6G rssi threshold */
		if (i4Argc >= 7 && kalStrtoint(apcArgv[6], 0,
			&prAdapter->rWtcModeInfo.cRssiThreshold_6G)) {
			DBGLOG(REQ, LOUD,
				"2.4G rssi threshold %s err\n", apcArgv[6]);
			return WLAN_STATUS_INVALID_DATA;
		} else {
			prAdapter->rWtcModeInfo.cRssiThreshold_6G = prWifiVar->cRssiThreshold_6G;
		}
#endif
	} else {
		DBGLOG(REQ, ERROR, "set WTC mode failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

#endif /* CFG_SUPPORT_NCHO */

#if CFG_SUPPORT_ASSURANCE
int testmode_set_disconnect_ies(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint16_t hexLength = (uint16_t)i4TotalLen - 20;
	uint8_t *asciiArray = NULL;
	uint8_t IEs[CFG_CFG80211_IE_BUF_LEN];
	uint16_t ieLength = 0;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc %i, cmd [%s]\n", i4Argc, apcArgv[1]);
		wlanHexStrToByteArray(asciiArray, IEs, CFG_CFG80211_IE_BUF_LEN);
		DBGLOG(REQ, INFO, "ielen:%d, hexlen:%d\n", ieLength, hexLength);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetDisconnectIes,
			IEs, ieLength, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, ERROR, "fail invalid data\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

int testmode_get_roaming_reason_enabled(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}

	/*<2> Query NCHOEnable Satus*/
	rStatus = kalIoctl(prGlueInfo, wlanoidGetRoamingReasonEnable,
			   &u4Param, sizeof(u4Param), &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, 512, "%u", u4Param);
	DBGLOG(REQ, INFO, "ret is %d\n", u4Param);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_roaming_reason_enabled(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint32_t u4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc %i, %s\n", i4Argc, apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "set enable cmd %d\n", u4Param);
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetRoamingReasonEnable,
				&u4Param, sizeof(uint32_t), &u4SetInfoLen);
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
		}
	} else {
		DBGLOG(REQ, ERROR, "set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}

int testmode_get_br_err_reason_enabled(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	char buf[512];

	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}

	/*<2> Query NCHOEnable Satus*/
	rStatus = kalIoctl(prGlueInfo, wlanoidGetBrErrReasonEnable,
			   &u4Param, sizeof(u4Param), &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
		return rStatus;
	}

	i4BytesWritten = snprintf(buf, 512, "%u", u4Param);
	DBGLOG(REQ, INFO, "ret is %d\n", u4Param);

	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_br_err_reason_enabled(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen)
{
	uint32_t u4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, TRACE, "command is %s\n", pcCommand);
	WIPHY_PRIV(wiphy, prGlueInfo);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "argc %i, %s\n", i4Argc, apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "set enable cmd %d\n", u4Param);
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetBrErrReasonEnable,
				&u4Param, sizeof(uint32_t), &u4SetInfoLen);
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, ERROR, "fail 0x%x\n", rStatus);
		}
	} else {
		DBGLOG(REQ, ERROR, "set failed\n");
		rStatus = WLAN_STATUS_INVALID_DATA;
	}

	return rStatus;
}
#endif

#if (CFG_SUPPORT_802_11BE_MLO == 1)
enum ENUM_BE_REASON {
	BE_REASON_SUCCESS = 0,
	BE_REASON_TEMP_ERROR = -1,
	BE_REASON_INVALID_ARGUMENT = -2,
	BE_REASON_TEMP_BUSY = -3,
	BE_REASON_NOT_SUPPORTED = -4,
	BE_REASON_NOT_AVAILABLE = -5,
};

int wlanStatus2Reason(uint32_t u4Status)
{
	switch (u4Status) {
	case WLAN_STATUS_SUCCESS:
		return BE_REASON_SUCCESS;
	case WLAN_STATUS_FAILURE:
		return BE_REASON_TEMP_ERROR;
	case WLAN_STATUS_INVALID_DATA:
		return BE_REASON_INVALID_ARGUMENT;
	case WLAN_STATUS_RESOURCES:
		return BE_REASON_TEMP_BUSY;
	case WLAN_STATUS_NOT_SUPPORTED:
		return BE_REASON_NOT_SUPPORTED;
	case WLAN_STATUS_NOT_ACCEPTED:
		return BE_REASON_NOT_AVAILABLE;
	default:
		return BE_REASON_TEMP_ERROR;
	}
};

int testmode_set_ml(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t u4Status;
	int reason = 0;
	int32_t i4BytesWritten = -1;
	char buf[512];

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo)
		prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -EINVAL;
	if (!glIsWiFi7CfgFile())
		return -EINVAL;

	if (prAdapter->rWifiVar.ucDisableFwkMlc) {
		DBGLOG(INIT, WARN, "Framework MLC cmd is disabled (%d)\n",
			prAdapter->rWifiVar.ucDisableFwkMlc);
		u4Status = WLAN_STATUS_NOT_ACCEPTED;
	} else {
		u4Status = testmode_set_ml_link_state(wiphy,
			wdev, pcCommand, i4TotalLen);
	}

	reason = wlanStatus2Reason(u4Status);
	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
		512 - i4BytesWritten, "%d\n", reason);
	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}

int testmode_set_wifi7_enable(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = -1;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus, u4Enable = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo)
		prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -EINVAL;
	if (!glIsWiFi7CfgFile())
		return -EINVAL;

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	if (i4Argc < 1) {
		DBGLOG(REQ, ERROR, "wrong input parameter %d\n", i4Argc);
		return -1;
	}

	DBGLOG(REQ, TRACE, "argc is %i, %s\n", i4Argc, apcArgv[1]);
	i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Enable);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
		       i4BytesWritten);
		return -1;
	}

	if (u4Enable) {
		prAdapter->rWifiVar.ucStaEht = FEATURE_ENABLED;
		prAdapter->rWifiVar.ucSta6gBandwidth = MAX_BW_320_2MHZ;
	} else {
		prAdapter->rWifiVar.ucStaEht = FEATURE_DISABLED;
		prAdapter->rWifiVar.ucSta6gBandwidth = MAX_BW_160MHZ;
	}

	return WLAN_STATUS_SUCCESS;
}

int testmode_set_num_allowed_mlo_link(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = -1;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus, u4Num = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo)
		prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -EINVAL;
	if (!glIsWiFi7CfgFile())
		return -EINVAL;

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	if (i4Argc < 1) {
		DBGLOG(REQ, ERROR, "wrong input parameter %d\n", i4Argc);
		return -1;
	}

	DBGLOG(REQ, TRACE, "argc is %i, %s\n", i4Argc, apcArgv[1]);
	i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Num);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "parse u4Param error %d\n",
		       i4BytesWritten);
		return -1;
	}

	if (u4Num > MLD_LINK_MAX) {
		DBGLOG(REQ, ERROR, "num_allowed_mlo_link %d > max %d\n",
		       u4Num, MLD_LINK_MAX);
		return -1;
	}

	if (u4Num == 0)
		prAdapter->rWifiVar.ucMldLinkMax = MLD_LINK_MAX;
	else
		prAdapter->rWifiVar.ucMldLinkMax = u4Num;

	return WLAN_STATUS_SUCCESS;
}

int testmode_measure_ml_chnl_condition(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	uint32_t u4Status;
	int reason = 0;
	int32_t i4BytesWritten = -1;
	char buf[512];

	u4Status = testmode_get_ml_chnl_condition(wiphy,
		wdev, pcCommand, i4TotalLen);

	reason = wlanStatus2Reason(u4Status);

	i4BytesWritten = 0;
	i4BytesWritten += snprintf(buf + i4BytesWritten,
		512 - i4BytesWritten, "%d\n", reason);
	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}
#endif /* CFG_SUPPORT_802_11BE_MLO */

#if CFG_SUPPORT_TX_ANT_CTRL
int testmode_set_tx_ant_mode(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	uint32_t u4BufLen, rStatus, u4AntId = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0;
	int32_t i4Ret = -1;
	uint8_t ucBssIdx = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rConfig = {0};
	uint8_t cmd[128] = {0};
	uint8_t strLen = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo)
		prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -EINVAL;
	ucBssIdx = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIdx))
		return -EINVAL;

	DBGLOG(REQ, TRACE, "command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "parse Arg failed, %u", rStatus);
		return rStatus;
	}
	if (i4Argc != 2) {
		DBGLOG(REQ, ERROR, "wrong input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}
	DBGLOG(REQ, TRACE, "argc is %i, %s\n", i4Argc, apcArgv[1]);

	i4Ret = kalkStrtou32(apcArgv[1], 0, &u4AntId);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "parse u4AntId error %d\n", i4Ret);
		return WLAN_STATUS_INVALID_DATA;
	}
	if (u4AntId > 3) {
		DBGLOG(REQ, ERROR, "Ant_id unexpected %u\n", u4AntId);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Ant_id mapping to ucAntMode
	 * 0: both Ant_1 & Ant_2 are active --> 3: ANT_MODE_2T2R
	 * 1: only Ant_1 is active for Tx --> 4: ANT_MODE_1T_CHAIN0_2R
	 * 2: only Ant_2 is active for Tx --> 5: ANT_MODE_1T_CHAIN1_2R
	 * 3: only the better Ant is active for Tx
	 *      --> 6: ANT_MODE_1T_BETTER_RSSI_2R
	 */
	strLen = kalSnprintf(cmd, sizeof(cmd), "AntControl %d %d",
			     EVENT_OPMODE_CHANGE_REASON_TX_ANT_CTRL,
			     u4AntId + 3);

	DBGLOG(REQ, INFO,
		"set_chip to FW %s, strlen=%d",
		cmd, strLen);
	rConfig.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
	rConfig.u2MsgSize = strLen;
	kalStrnCpy(rConfig.aucCmd, cmd, strLen);
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetChipConfig,
		(void *)&rConfig,
		sizeof(struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT), &u4BufLen,
		ucBssIdx);
	return rStatus;
}
int testmode_get_tx_ant_mode(struct wiphy *wiphy,
	struct wireless_dev *wdev, char *pcCommand, int i4TotalLen)
{
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = -1;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint8_t ucBssIdx = 0;
	uint32_t u4BufLen, rStatus;
	uint8_t cmd[128] = {0};
	uint8_t strLen = 0;
	char buf[512];
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rConfig = {0};
	uint32_t u4AntMode = ENUM_WF_NON_FAVOR;
	uint8_t ucAntIdx = 0;

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo)
		prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -EINVAL;

	ucBssIdx = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIdx))
		return -EINVAL;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "parse Arg failed, %u", rStatus);
		return rStatus;
	}
	if (i4Argc != 1) {
		DBGLOG(REQ, ERROR, "wrong input parameter %d\n", i4Argc);
		return WLAN_STATUS_INVALID_DATA;
	}

	strLen = kalSnprintf(cmd, sizeof(cmd), "TxAntCtrl");

	rConfig.ucType = CHIP_CONFIG_TYPE_ASCII;
	rConfig.u2MsgSize = strLen;
	kalStrnCpy(rConfig.aucCmd, cmd, strLen);
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidQueryChipConfig,
		(void *)&rConfig,
		sizeof(struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT), &u4BufLen,
		ucBssIdx);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ioctrl failed, %u", rStatus);
		return rStatus;
	}

	DBGLOG(REQ, TRACE, "return buf %s, length %u\n",
		rConfig.aucCmd, kalStrLen(rConfig.aucCmd));

	kalMemZero(cmd, sizeof(cmd));
	strLen = kalSnprintf(cmd, sizeof(cmd), "TxAntCtrl = ");

	if (kalStrnCmp(rConfig.aucCmd, cmd, strLen) == 0) {
		kalkStrtou32(rConfig.aucCmd + strLen, 0, &u4AntMode);
		DBGLOG(REQ, TRACE, "in str cmp: %d", u4AntMode);
	}

	if (u4AntMode == ENUM_WF_0_ONE_STREAM_PATH_FAVOR)
		ucAntIdx = 1;
	else if (u4AntMode == ENUM_WF_1_ONE_STREAM_PATH_FAVOR)
		ucAntIdx = 2;

	DBGLOG(REQ, TRACE, "final u4AntMode %u, ucAntIdx %d", u4AntMode,
	       ucAntIdx);

	i4BytesWritten = 0;
	i4BytesWritten += kalSnprintf(buf + i4BytesWritten,
		sizeof(buf) - i4BytesWritten, "%d\n", ucAntIdx);
	return mtk_cfg80211_process_str_cmd_reply(wiphy,
		buf, i4BytesWritten + 1);
}
#endif /* CFG_SUPPORT_TX_ANT_CTRL */

struct STR_CMD_HANDLER str_cmd_ext_handlers[] = {
#if WLAN_INCLUDE_SYS
	{
		.pcCmdStr  = CMD_SETWSECINFO,
		.pfHandler = testmode_wifi_safe_mode,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_SET_FCC_CHANNEL,
		.pfHandler = testmode_wifi_fcc_channel,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = NULL,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
#endif
	{
		.pcCmdStr  = CMD_SET_INDOOR_CHANNELS,
		.pfHandler = testmode_set_indoor_ch,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
#if CFG_SUPPORT_LLW_SCAN && (CFG_EXT_VERSION < 3)
	{
		.pcCmdStr  = CMD_LATENCY_CRT_DATA_SET,
		.pfHandler = testmode_set_latency_crt_data,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_DWELL_TIME_SET,
		.pfHandler = testmode_set_scan_param,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(5),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
#endif
#if CFG_SUPPORT_SCAN_LOG
	{
		.pcCmdStr  = CMD_BCN_RECV_START,
		.pfHandler = testmode_set_bcn_recv_start,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_BCN_RECV_STOP,
		.pfHandler = testmode_set_bcn_recv_stop,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = NULL,
		.u4PolicySize = 0
	},
#endif
#ifdef CFG_MTK_CONNSYS_DEDICATED_LOG_PATH
	{
		.pcCmdStr  = CMD_DEBUG_LEVEL_SET,
		.pfHandler = testmode_set_debug_level,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u8_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u8_ext_policy)
	},
#endif
#if CFG_SAP_RPS_SUPPORT
	{
		.pcCmdStr  = CMD_ENABLE_SAP_RPS,
		.pfHandler = testmode_set_ap_rps,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = rps_force_set_policy
	},
	{
		.pcCmdStr  = CMD_SET_SAP_RPS_PARAM,
		.pfHandler = testmode_set_rps_param,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(6),
		.policy    = rps_param_policy
	},
#endif
#if CFG_SAP_SUS_SUPPORT
	{
		.pcCmdStr  = CMD_SET_SAP_SUS,
		.pfHandler = testmode_set_ap_sus,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = sap_sus_policy
	},
#endif
#if 0
	{
		.pcCmdStr  = CMD_GET_STA_TRX_INFO,
		.pfHandler = testmode_get_sta_trx_info,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = sta_info_policy,
		.u4PolicySize = ARRAY_SIZE(sta_info_policy)
	},
#endif
#if CFG_SUPPORT_MANIPULATE_TID
	{
		.pcCmdStr  = CMD_MANIPULATE_TID_SET,
		.pfHandler = testmode_manipulate_tid,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(4),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
#endif
#if CFG_STAINFO_FEATURE
	{
		.pcCmdStr  = CMD_GET_BSSINFO,
		.pfHandler = testmode_get_bssinfo,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_GET_STAINFO,
		.pfHandler = testmode_get_sta_info,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = sta_info_policy,
		.u4PolicySize = ARRAY_SIZE(sta_info_policy)
	},
	{
		.pcCmdStr  = CMD_GET_CONN_REJECT_INFO,
		.pfHandler = testmode_get_conn_reject_info,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
#endif
#if CFG_SUPPORT_BW_SELECT
	{
		.pcCmdStr  = CMD_SET_MAX_BW,
		.pfHandler = testmode_set_max_bw,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = set_max_bw_policy,
		.u4PolicySize = ARRAY_SIZE(set_max_bw_policy)
	},
	{
		.pcCmdStr  = CMD_GET_MAX_BW,
		.pfHandler = testmode_get_max_bw,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
#endif
#if (CFG_SUPPORT_TWT == 1)
#if 0
	{
		.pcCmdStr  = CMD_TWT_SETUP,
		.pfHandler = twt_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_TWT_TEARDOWN,
		.pfHandler = twt_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_GET_TWT_CAP,
		.pfHandler = twt_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_GET_TWT_STATUS,
		.pfHandler = twt_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_SCHED_PM_SETUP,
		.pfHandler = scheduledpm_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_SCHED_PM_TEARDOWN,
		.pfHandler = scheduledpm_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_GET_SCHED_PM_STATUS,
		.pfHandler = scheduledpm_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_SCHED_PM_SUSPEND,
		.pfHandler = scheduledpm_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_SCHED_PM_RESUME,
		.pfHandler = scheduledpm_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_LEAKY_AP_ACTIVE_DETECT,
		.pfHandler = scheduledpm_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_LEAKY_AP_PASSIVE_DETECT_START,
		.pfHandler = scheduledpm_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_LEAKY_AP_PASSIVE_DETECT_END,
		.pfHandler = scheduledpm_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_LEAKY_AP_GRACE_PERIOD,
		.pfHandler = scheduledpm_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
#endif
	{
		.pcCmdStr  = CMD_SET_DELAYED_WAKEUP,
		.pfHandler = delay_wakeup_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_SET_DELAYED_WAKEUP_TYPE,
		.pfHandler = delay_wakeup_set_para,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
#endif
#if CFG_SUPPORT_NCHO
	{
		.pcCmdStr  = CMD_NCHO_ROAM_TRIGGER_SET,
		.pfHandler = testmode_set_ncho_roam_trigger,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = set_roam_trigger_ext_policy,
		.u4PolicySize = ARRAY_SIZE(set_roam_trigger_ext_policy)
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_TRIGGER_GET,
		.pfHandler = testmode_get_ncho_roam_trigger,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_DELTA_SET,
		.pfHandler = testmode_set_ncho_roam_delta,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_DELTA_GET,
		.pfHandler = testmode_get_ncho_roam_delta,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_PERIOD_SET,
		.pfHandler = testmode_set_ncho_roam_scn_period,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_PERIOD_GET,
		.pfHandler = testmode_get_ncho_roam_scn_period,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_CHANNELS_SET,
		.pfHandler = testmode_set_ncho_roam_scn_chnl_set,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_CHANNELS_ADD,
		.pfHandler = testmode_set_ncho_roam_scn_chnl_add,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_CHANNELS_GET,
		.pfHandler = testmode_get_ncho_roam_scn_chnl,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_CONTROL_SET,
		.pfHandler = testmode_set_ncho_roam_scn_ctrl,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_CONTROL_GET,
		.pfHandler = testmode_get_ncho_roam_scn_ctrl,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_NCHO_MODE_SET,
		.pfHandler = testmode_set_ncho_mode,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_NCHO_MODE_GET,
		.pfHandler = testmode_get_ncho_mode,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_FREQS_GET,
		.pfHandler = testmode_get_ncho_roam_scn_freq,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_FREQS_SET,
		.pfHandler = testmode_set_ncho_roam_scn_freq_set,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_SCAN_FREQS_ADD,
		.pfHandler = testmode_set_ncho_roam_scn_freq_add,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_BAND_GET,
		.pfHandler = testmode_get_ncho_roam_band,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_NCHO_ROAM_BAND_SET,
		.pfHandler = testmode_set_ncho_roam_band,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_GET_CU,
		.pfHandler = testmode_get_cu,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_REASSOC_FREQUENCY_LEGACY,
		.pfHandler = testmode_reassoc1,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = reassoc_ext_policy,
		.u4PolicySize = ARRAY_SIZE(reassoc_ext_policy)
	},
	{
		.pcCmdStr  = CMD_REASSOC_FREQUENCY,
		.pfHandler = testmode_reassoc0,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = reassoc_ext_policy,
		.u4PolicySize = ARRAY_SIZE(reassoc_ext_policy)
	},
	{
		.pcCmdStr  = CMD_REASSOC_LEGACY,
		.pfHandler = testmode_reassoc3,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = reassoc_ext_policy,
		.u4PolicySize = ARRAY_SIZE(reassoc_ext_policy)
	},
	{
		.pcCmdStr  = CMD_REASSOC2,
		.pfHandler = testmode_reassoc2,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(3),
		.policy    = reassoc_ext_policy,
		.u4PolicySize = ARRAY_SIZE(reassoc_ext_policy)
	},
	{
		.pcCmdStr  = CMD_ADDROAMSCANCHANNELS_LEGACY,
		.pfHandler = testmode_add_roam_scn_chnl,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_GETROAMSCANCHANNELS_LEGACY,
		.pfHandler = testmode_get_roam_scn_chnl,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_GETROAMSCANFREQUENCIES_LEGACY,
		.pfHandler = testmode_get_roam_scn_freq,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_ADDROAMSCANFREQUENCIES_LEGACY,
		.pfHandler = testmode_add_roam_scn_freq,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_SETROAMTRIGGER_LEGACY,
		.pfHandler = testmode_set_roam_trigger,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = set_roam_trigger_ext_policy,
		.u4PolicySize = ARRAY_SIZE(set_roam_trigger_ext_policy)
	},
	{
		.pcCmdStr  = CMD_GETROAMTRIGGER_LEGACY,
		.pfHandler = testmode_get_roam_trigger,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_SET_WTC_MODE,
		.pfHandler = testmode_set_wtc_mode,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(7),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
#endif
#if CFG_SUPPORT_ASSURANCE
	{
		.pcCmdStr  = CMD_SET_DISCONNECT_IES,
		.pfHandler = testmode_set_disconnect_ies,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_GET_ROAMING_REASON_ENABLED,
		.pfHandler = testmode_get_roaming_reason_enabled,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_SET_ROAMING_REASON_ENABLED,
		.pfHandler = testmode_set_roaming_reason_enabled,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
	{
		.pcCmdStr  = CMD_GET_BR_ERR_REASON_ENABLED,
		.pfHandler = testmode_get_br_err_reason_enabled,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL,
		.u4PolicySize = 0
	},
	{
		.pcCmdStr  = CMD_SET_BR_ERR_REASON_ENABLED,
		.pfHandler = testmode_set_br_err_reason_enabled,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = u32_ext_policy,
		.u4PolicySize = ARRAY_SIZE(u32_ext_policy)
	},
#endif
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	{
		.pcCmdStr  = CMD_SET_ML,
		.pfHandler = testmode_set_ml,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = NULL
	},
	{
		.pcCmdStr  = CMD_GET_ML,
		.pfHandler = testmode_get_ml_link_state,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL
	},
	{
		.pcCmdStr  = CMD_SET_WIFI7_ENABLE,
		.pfHandler = testmode_set_wifi7_enable,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(2),
		.policy    = NULL
	},
	{
		.pcCmdStr  = CMD_SET_NUM_ALLOWED_MLO_LINK,
		.pfHandler = testmode_set_num_allowed_mlo_link,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(2),
		.policy    = NULL
	},
	{
		.pcCmdStr  = CMD_MEASURE_ML_CHANNEL,
		.pfHandler = testmode_measure_ml_chnl_condition,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL
	},
#endif /* CFG_SUPPORT_802_11BE_MLO */
#if CFG_SUPPORT_TX_ANT_CTRL
	{
		.pcCmdStr  = CMD_SET_TX_ANT_MODE,
		.pfHandler = testmode_set_tx_ant_mode,
		.argPolicy = VERIFY_MIN_ARG_NUM,
		.ucArgNum  = COMMON_CMD_SET_ARG_NUM(2),
		.policy    = NULL
	},
	{
		.pcCmdStr  = CMD_GET_TX_ANT_MODE,
		.pfHandler = testmode_get_tx_ant_mode,
		.argPolicy = VERIFY_EXACT_ARG_NUM,
		.ucArgNum  = COMMON_CMD_GET_ARG_NUM(1),
		.policy    = NULL
	},
#endif /* CFG_SUPPORT_TX_ANT_CTRL */
};

PRIV_CMD_FUNCTION get_priv_cmd_ext_handler(uint8_t *cmd, int32_t len)
{
	uint8_t ucIdx = 0;
	uint32_t ret = WLAN_STATUS_FAILURE;
	int8_t *pcCmd;
	int32_t i4CmdSize;
	uint32_t cmdLen;

	for (ucIdx = 0; ucIdx < sizeof(priv_cmd_ext_handlers) / sizeof(struct
				PRIV_CMD_HANDLER); ucIdx++) {
		cmdLen = strlen(priv_cmd_ext_handlers[ucIdx].pcCmdStr);
		if (len >= cmdLen &&
			strnicmp(cmd, priv_cmd_ext_handlers[ucIdx].pcCmdStr,
			     cmdLen) == 0) {
			/* skip the pcCmdStr without cmd postfix */
			if (cmd[cmdLen] != '\0' && cmd[cmdLen] != '\n' &&
				cmd[cmdLen] != ' ')
				continue;

			i4CmdSize = len + 1;
			pcCmd = (int8_t *) kalMemAlloc(i4CmdSize, VIR_MEM_TYPE);
			if (!pcCmd) {
				DBGLOG(REQ, WARN,
					"%s, alloc mem failed\n", __func__);
				return 0;
			}
			kalMemZero(pcCmd, i4CmdSize);
			kalMemCopy(pcCmd, cmd, len);
			pcCmd[i4CmdSize - 1] = '\0';

			DBGLOG(REQ, LOUD,
				"ioctl priv command is [%s], argPolicy[%d] argNum[%d] u4PolicySize[%d]\n",
				pcCmd,
				priv_cmd_ext_handlers[ucIdx].argPolicy,
				priv_cmd_ext_handlers[ucIdx].ucArgNum,
				priv_cmd_ext_handlers[ucIdx].u4PolicySize);

			ret = cmd_validate(pcCmd,
				priv_cmd_ext_handlers[ucIdx].argPolicy,
				priv_cmd_ext_handlers[ucIdx].ucArgNum,
				priv_cmd_ext_handlers[ucIdx].policy,
				priv_cmd_ext_handlers[ucIdx].u4PolicySize);

			if (pcCmd)
				kalMemFree(pcCmd, VIR_MEM_TYPE, i4CmdSize);

			if (ret != WLAN_STATUS_SUCCESS) {
				DBGLOG(REQ, WARN, "Command validate fail\n");
				return NULL;
			}
			return priv_cmd_ext_handlers[ucIdx].pfHandler;
		}
	}
	return NULL;
}

STR_CMD_FUNCTION get_str_cmd_ext_handler(uint8_t *cmd, int32_t len)
{
	uint8_t ucIdx = 0;
	uint32_t ret = WLAN_STATUS_FAILURE;
	int8_t *pcCmd;
	int32_t i4CmdSize;
	uint32_t cmdLen;

	for (ucIdx = 0; ucIdx < sizeof(str_cmd_ext_handlers) / sizeof(struct
				STR_CMD_HANDLER); ucIdx++) {
		cmdLen = strlen(str_cmd_ext_handlers[ucIdx].pcCmdStr);
		if (len >= cmdLen &&
			strnicmp(cmd, str_cmd_ext_handlers[ucIdx].pcCmdStr,
				 cmdLen) == 0) {
			/* skip the pcCmdStr without cmd postfix */
			if (cmd[cmdLen] != '\0' && cmd[cmdLen] != ' ')
				continue;

			i4CmdSize = len + 1;
			pcCmd = (int8_t *) kalMemAlloc(i4CmdSize, VIR_MEM_TYPE);
			if (!pcCmd) {
				DBGLOG(REQ, WARN,
					"%s, alloc mem failed\n", __func__);
				return 0;
			}
			kalMemZero(pcCmd, i4CmdSize);
			kalMemCopy(pcCmd, cmd, len);
			pcCmd[i4CmdSize - 1] = '\0';

			DBGLOG(REQ, LOUD,
				"vendor str command is [%s], argPolicy[%d] argNum[%d] u4PolicySize[%d]\n",
				pcCmd,
				str_cmd_ext_handlers[ucIdx].argPolicy,
				str_cmd_ext_handlers[ucIdx].ucArgNum,
				str_cmd_ext_handlers[ucIdx].u4PolicySize);

			ret = cmd_validate(pcCmd,
				str_cmd_ext_handlers[ucIdx].argPolicy,
				str_cmd_ext_handlers[ucIdx].ucArgNum,
				str_cmd_ext_handlers[ucIdx].policy,
				str_cmd_ext_handlers[ucIdx].u4PolicySize);

			if (pcCmd)
				kalMemFree(pcCmd, VIR_MEM_TYPE, i4CmdSize);

			if (ret != WLAN_STATUS_SUCCESS) {
				DBGLOG(REQ, WARN, "Command validate fail\n");
				return NULL;
			}
			return str_cmd_ext_handlers[ucIdx].pfHandler;
		}
	}
	return NULL;
}
#endif
#endif

