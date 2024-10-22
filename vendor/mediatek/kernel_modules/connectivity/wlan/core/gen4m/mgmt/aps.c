/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "precomp.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*
 * definition for AP selection algrithm
 */
#define BSS_FULL_SCORE                          (100)
#define CHNL_BSS_NUM_THRESOLD                   100
#define BSS_STA_CNT_THRESOLD                    30
#define SCORE_PER_AP                            1
#define ROAMING_NO_SWING_SCORE_STEP             100
/* MCS9 at BW 160 requires rssi at least -48dbm */
#define BEST_RSSI                               -48
/* MCS7 at 20BW, MCS5 at 40BW, MCS4 at 80BW, MCS3 at 160BW */
#define GOOD_RSSI_FOR_HT_VHT                    -64
/* Link speed 1Mbps need at least rssi -94dbm for 2.4G */
#define MINIMUM_RSSI_2G4                        -94
/* Link speed 6Mbps need at least rssi -86dbm for 5G */
#define MINIMUM_RSSI_5G                         -86
#if (CFG_SUPPORT_WIFI_6G == 1)
/* Link speed 6Mbps need at least rssi -86dbm for 6G */
#define MINIMUM_RSSI_6G                         -86
#endif

/* level of rssi range on StatusBar */
#define RSSI_MAX_LEVEL                          -55
#define RSSI_SECOND_LEVEL                       -66

/* Real Rssi of a Bss may range in current_rssi - 5 dbm
 *to current_rssi + 5 dbm
 */
#define RSSI_DIFF_BIG_STEP			15 /* dbm */
#define RSSI_DIFF_MED_STEP			10 /* dbm */
#define RSSI_DIFF_SML_STEP			5 /* dbm */
#define LOW_RSSI_FOR_5G_BAND                    -70 /* dbm */
#define HIGH_RSSI_FOR_5G_BAND                   -60 /* dbm */

#define CHNL_DWELL_TIME_DEFAULT  100
#define CHNL_DWELL_TIME_ONLINE   50

#define WEIGHT_IDX_CHNL_UTIL                    0
#define WEIGHT_IDX_RSSI                         2
#define WEIGHT_IDX_SCN_MISS_CNT                 2
#define WEIGHT_IDX_PROBE_RSP                    1
#define WEIGHT_IDX_CLIENT_CNT                   0
#define WEIGHT_IDX_AP_NUM                       0
#define WEIGHT_IDX_5G_BAND                      2
#define WEIGHT_IDX_BAND_WIDTH                   1
#define WEIGHT_IDX_STBC                         1
#define WEIGHT_IDX_DEAUTH_LAST                  1
#define WEIGHT_IDX_BLOCK_LIST                   2
#define WEIGHT_IDX_SAA                          0
#define WEIGHT_IDX_CHNL_IDLE                    1
#define WEIGHT_IDX_OPCHNL                       0
#define WEIGHT_IDX_TPUT                         1
#define WEIGHT_IDX_PREFERENCE                   2

#define WEIGHT_IDX_CHNL_UTIL_PER                0
#define WEIGHT_IDX_RSSI_PER                     4
#define WEIGHT_IDX_SCN_MISS_CNT_PER             4
#define WEIGHT_IDX_PROBE_RSP_PER                1
#define WEIGHT_IDX_CLIENT_CNT_PER               1
#define WEIGHT_IDX_AP_NUM_PER                   6
#define WEIGHT_IDX_5G_BAND_PER                  4
#define WEIGHT_IDX_BAND_WIDTH_PER               1
#define WEIGHT_IDX_STBC_PER                     1
#define WEIGHT_IDX_DEAUTH_LAST_PER              1
#define WEIGHT_IDX_BLOCK_LIST_PER               4
#define WEIGHT_IDX_SAA_PER                      1
#define WEIGHT_IDX_CHNL_IDLE_PER                6
#define WEIGHT_IDX_OPCHNL_PER                   6
#define WEIGHT_IDX_TPUT_PER                     2
#define WEIGHT_IDX_PREFERENCE_PER               2

#define ROAM_SCORE_DELTA                        5
#define BSS_MATCH_BSSID_SCORE			(30000)
#define BSS_MATCH_BSSID_HINT_SCORE		(20000)

#define APS_AMSDU_HT_3K                         (3839)
#define APS_AMSDU_HT_8K                         (7935)
#define APS_AMSDU_VHT_HE_EHT_3K                 (3895)
#define APS_AMSDU_VHT_HE_EHT_8K                 (7991)
#define APS_AMSDU_VHT_HE_EHT_11K                (11454)

#define WEIGHT_GBAND_COEX_DOWNGRADE		70 /* 0~100 */
#define CU_6G_INDEX_OFFSET			256

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

struct WEIGHT_CONFIG {
	uint8_t ucChnlUtilWeight;
	uint8_t ucSnrWeight;
	uint8_t ucRssiWeight;
	uint8_t ucProbeRespWeight;
	uint8_t ucClientCntWeight;
	uint8_t ucApNumWeight;
	uint8_t ucBandWeight;
	uint8_t ucBandWidthWeight;
	uint8_t ucStbcWeight;
	uint8_t ucLastDeauthWeight;
	uint8_t ucBlockListWeight;
	uint8_t ucSaaWeight;
	uint8_t ucChnlIdleWeight;
	uint8_t ucOpchnlWeight;
	uint8_t ucTputWeight;
	uint8_t ucPreferenceWeight;
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

enum ENUM_APS_REPLACE_REASON {
	/* reason to not replace */
	APS_LOW_SCORE = 0,
	APS_UNMATCH_BSSID,
	APS_UNMATCH_BSSID_HINT,
	APS_WORSE_RSSI,

	/* reason to replace */
	APS_FIRST_CANDIDATE,
	APS_HIGH_SCORE,
	APS_MATCH_BSSID,
	APS_MATCH_BSSID_HINT,
	APS_BETTER_RSSI,

	/* don't add after this */
	APS_REPLACE_REASON_NUM,
	APS_NEED_REPLACE = APS_FIRST_CANDIDATE,
};

/* mapping with ENUM_APS_REPLACE_REASON */
static const uint8_t *apucReplaceReasonStr[APS_REPLACE_REASON_NUM] = {
	(uint8_t *) DISP_STRING("LOW SCORE"),
	(uint8_t *) DISP_STRING("UNMATCH BSSID"),
	(uint8_t *) DISP_STRING("UNMATCH BSSID_HINT"),
	(uint8_t *) DISP_STRING("WORSE RSSI"),
	(uint8_t *) DISP_STRING("FIRST CANDIDATE"),
	(uint8_t *) DISP_STRING("HIGH SCORE"),
	(uint8_t *) DISP_STRING("MATCH BSSID"),
	(uint8_t *) DISP_STRING("MATCH BSSID_HINT"),
	(uint8_t *) DISP_STRING("BETTER RSSI"),
};

struct WEIGHT_CONFIG gasMtkWeightConfig[ROAM_TYPE_NUM] = {
	[ROAM_TYPE_RCPI] = {
		.ucChnlUtilWeight = WEIGHT_IDX_CHNL_UTIL,
		.ucRssiWeight = WEIGHT_IDX_RSSI,
		.ucProbeRespWeight = WEIGHT_IDX_PROBE_RSP,
		.ucClientCntWeight = WEIGHT_IDX_CLIENT_CNT,
		.ucApNumWeight = WEIGHT_IDX_AP_NUM,
		.ucBandWeight = WEIGHT_IDX_5G_BAND,
		.ucBandWidthWeight = WEIGHT_IDX_BAND_WIDTH,
		.ucStbcWeight = WEIGHT_IDX_STBC,
		.ucLastDeauthWeight = WEIGHT_IDX_DEAUTH_LAST,
		.ucBlockListWeight = WEIGHT_IDX_BLOCK_LIST,
		.ucSaaWeight = WEIGHT_IDX_SAA,
		.ucChnlIdleWeight = WEIGHT_IDX_CHNL_IDLE,
		.ucOpchnlWeight = WEIGHT_IDX_OPCHNL,
		.ucTputWeight = WEIGHT_IDX_TPUT,
		.ucPreferenceWeight = WEIGHT_IDX_PREFERENCE
	}
#if CFG_SUPPORT_ROAMING
	, [ROAM_TYPE_PER] = {
		.ucChnlUtilWeight = WEIGHT_IDX_CHNL_UTIL_PER,
		.ucRssiWeight = WEIGHT_IDX_RSSI_PER,
		.ucProbeRespWeight = WEIGHT_IDX_PROBE_RSP_PER,
		.ucClientCntWeight = WEIGHT_IDX_CLIENT_CNT_PER,
		.ucApNumWeight = WEIGHT_IDX_AP_NUM_PER,
		.ucBandWeight = WEIGHT_IDX_5G_BAND_PER,
		.ucBandWidthWeight = WEIGHT_IDX_BAND_WIDTH_PER,
		.ucStbcWeight = WEIGHT_IDX_STBC_PER,
		.ucLastDeauthWeight = WEIGHT_IDX_DEAUTH_LAST_PER,
		.ucBlockListWeight = WEIGHT_IDX_BLOCK_LIST_PER,
		.ucSaaWeight = WEIGHT_IDX_SAA_PER,
		.ucChnlIdleWeight = WEIGHT_IDX_CHNL_IDLE_PER,
		.ucOpchnlWeight = WEIGHT_IDX_OPCHNL_PER,
		.ucTputWeight = WEIGHT_IDX_TPUT_PER,
		.ucPreferenceWeight = WEIGHT_IDX_PREFERENCE_PER
	}
#endif
};

static uint8_t *apucBandStr[BAND_NUM] = {
	(uint8_t *) DISP_STRING("NULL"),
	(uint8_t *) DISP_STRING("2.4G"),
	(uint8_t *) DISP_STRING("5G"),
#if (CFG_SUPPORT_WIFI_6G == 1)
	(uint8_t *) DISP_STRING("6G")
#endif
};

const uint8_t *apuceLinkPlanStr[MLO_LINK_PLAN_NUM] = {
	"2G",
	"5G",
	"2G_5G",
	"5G_5G",
	"2G_5G_5G",
#if (CFG_SUPPORT_WIFI_6G == 1)
	"6G",
	"2G_6G",
	"5G_6G",
	"6G_6G",
	"2G_5G_6G",
	"2G_6G_6G",
#endif
};

static uint8_t aucBaSizeTranslate[8] = {
	[0] = 0,
	[1] = 2,
	[2] = 4,
	[3] = 6,
	[4] = 8,
	[5] = 16,
	[6] = 32,
	[7] = 64
};

#if (CFG_SUPPORT_AVOID_DESENSE == 1)
const struct WFA_DESENSE_CHANNEL_LIST desenseChList[BAND_NUM] = {
	[BAND_5G]  = {120, 157},
#if (CFG_SUPPORT_WIFI_6G == 1)
	[BAND_6G]  = {13,  53},
#endif
};
#endif

#define PERCENTAGE(_val, _base) (_val * 100 / _base)

enum ENUM_BAND g_aeLinkPlan[MLO_LINK_PLAN_NUM][APS_LINK_MAX] = {
	{BAND_2G4, BAND_NULL, BAND_NULL},
	{BAND_5G, BAND_NULL, BAND_NULL},
	{BAND_2G4, BAND_5G, BAND_NULL},
	{BAND_5G, BAND_5G, BAND_NULL},
	{BAND_2G4, BAND_5G, BAND_5G},
#if (CFG_SUPPORT_WIFI_6G == 1)
	{BAND_6G, BAND_NULL, BAND_NULL},
	{BAND_2G4, BAND_6G, BAND_NULL},
	{BAND_5G, BAND_6G, BAND_NULL},
	{BAND_6G, BAND_6G, BAND_NULL},
	{BAND_2G4, BAND_5G, BAND_6G},
	{BAND_2G4, BAND_6G, BAND_6G},
#endif
};

/* Minimum SNR required to achieve a certain bitrate. */
struct minsnr_bitrate_entry {
	int minsnr;
	unsigned int bitrate; /* in Mbps */
};

/* VHT needs to be enabled in order to achieve MCS8 and MCS9 rates. */
static const int vht_mcs = 8;

static const struct minsnr_bitrate_entry vht20_table[] = {
	{ 0, 0 },
	{ 2, 6500 },   /* HT20 MCS0 */
	{ 5, 13000 },  /* HT20 MCS1 */
	{ 9, 19500 },  /* HT20 MCS2 */
	{ 11, 26000 }, /* HT20 MCS3 */
	{ 15, 39000 }, /* HT20 MCS4 */
	{ 18, 52000 }, /* HT20 MCS5 */
	{ 20, 58500 }, /* HT20 MCS6 */
	{ 25, 65000 }, /* HT20 MCS7 */
	{ 29, 78000 }, /* VHT20 MCS8 */
	{ -1, 78000 }  /* SNR > 29 */
};

static const struct minsnr_bitrate_entry vht40_table[] = {
	{ 0, 0 },
	{ 5, 13500 },   /* HT40 MCS0 */
	{ 8, 27000 },   /* HT40 MCS1 */
	{ 12, 40500 },  /* HT40 MCS2 */
	{ 14, 54000 },  /* HT40 MCS3 */
	{ 18, 81000 },  /* HT40 MCS4 */
	{ 21, 108000 }, /* HT40 MCS5 */
	{ 23, 121500 }, /* HT40 MCS6 */
	{ 28, 135000 }, /* HT40 MCS7 */
	{ 32, 162000 }, /* VHT40 MCS8 */
	{ 34, 180000 }, /* VHT40 MCS9 */
	{ -1, 180000 }  /* SNR > 34 */
};

static const struct minsnr_bitrate_entry vht80_table[] = {
	{ 0, 0 },
	{ 8, 29300 },   /* VHT80 MCS0 */
	{ 11, 58500 },  /* VHT80 MCS1 */
	{ 15, 87800 },  /* VHT80 MCS2 */
	{ 17, 117000 }, /* VHT80 MCS3 */
	{ 21, 175500 }, /* VHT80 MCS4 */
	{ 24, 234000 }, /* VHT80 MCS5 */
	{ 26, 263300 }, /* VHT80 MCS6 */
	{ 31, 292500 }, /* VHT80 MCS7 */
	{ 35, 351000 }, /* VHT80 MCS8 */
	{ 37, 390000 }, /* VHT80 MCS9 */
	{ -1, 390000 }  /* SNR > 37 */
};


static const struct minsnr_bitrate_entry vht160_table[] = {
	{ 0, 0 },
	{ 11, 58500 },  /* VHT160 MCS0 */
	{ 14, 117000 }, /* VHT160 MCS1 */
	{ 18, 175500 }, /* VHT160 MCS2 */
	{ 20, 234000 }, /* VHT160 MCS3 */
	{ 24, 351000 }, /* VHT160 MCS4 */
	{ 27, 468000 }, /* VHT160 MCS5 */
	{ 29, 526500 }, /* VHT160 MCS6 */
	{ 34, 585000 }, /* VHT160 MCS7 */
	{ 38, 702000 }, /* VHT160 MCS8 */
	{ 40, 780000 }, /* VHT160 MCS9 */
	{ -1, 780000 }  /* SNR > 37 */
};

/* EHT needs to be enabled in order to achieve MCS12 and MCS13 rates. */
#define EHT_MCS 12

static const struct minsnr_bitrate_entry he20_table[] = {
	{ 0, 0 },
	{ 2, 8600 },    /* HE20 MCS0 */
	{ 5, 17200 },   /* HE20 MCS1 */
	{ 9, 25800 },   /* HE20 MCS2 */
	{ 11, 34400 },  /* HE20 MCS3 */
	{ 15, 51600 },  /* HE20 MCS4 */
	{ 18, 68800 },  /* HE20 MCS5 */
	{ 20, 77400 },  /* HE20 MCS6 */
	{ 25, 86000 },  /* HE20 MCS7 */
	{ 29, 103200 }, /* HE20 MCS8 */
	{ 31, 114700 }, /* HE20 MCS9 */
	{ 34, 129000 }, /* HE20 MCS10 */
	{ 36, 143400 }, /* HE20 MCS11 */
	{ 39, 154900 }, /* EHT20 MCS12 */
	{ 42, 172100 }, /* EHT20 MCS13 */
	{ -1, 172100 }  /* SNR > 42 */
};

static const struct minsnr_bitrate_entry he40_table[] = {
	{ 0, 0 },
	{ 5, 17200 },   /* HE40 MCS0 */
	{ 8, 34400 },   /* HE40 MCS1 */
	{ 12, 51600 },  /* HE40 MCS2 */
	{ 14, 68800 },  /* HE40 MCS3 */
	{ 18, 103200 }, /* HE40 MCS4 */
	{ 21, 137600 }, /* HE40 MCS5 */
	{ 23, 154900 }, /* HE40 MCS6 */
	{ 28, 172100 }, /* HE40 MCS7 */
	{ 32, 206500 }, /* HE40 MCS8 */
	{ 34, 229400 }, /* HE40 MCS9 */
	{ 37, 258100 }, /* HE40 MCS10 */
	{ 39, 286800 }, /* HE40 MCS11 */
	{ 42, 309500 }, /* EHT40 MCS12 */
	{ 45, 344100 }, /* EHT40 MCS13 */
	{ -1, 344100 }  /* SNR > 45 */
};

static const struct minsnr_bitrate_entry he80_table[] = {
	{ 0, 0 },
	{ 8, 36000 },   /* HE80 MCS0 */
	{ 11, 72100 },  /* HE80 MCS1 */
	{ 15, 108100 }, /* HE80 MCS2 */
	{ 17, 144100 }, /* HE80 MCS3 */
	{ 21, 216200 }, /* HE80 MCS4 */
	{ 24, 288200 }, /* HE80 MCS5 */
	{ 26, 324300 }, /* HE80 MCS6 */
	{ 31, 360300 }, /* HE80 MCS7 */
	{ 35, 432400 }, /* HE80 MCS8 */
	{ 37, 480400 }, /* HE80 MCS9 */
	{ 40, 540400 }, /* HE80 MCS10 */
	{ 42, 600500 }, /* HE80 MCS11 */
	{ 45, 648500 }, /* EHT80 MCS12 */
	{ 48, 720600 }, /* EHT80 MCS13 */
	{ -1, 720600 }  /* SNR > 48 */
};


static const struct minsnr_bitrate_entry he160_table[] = {
	{ 0, 0 },
	{ 11, 72100 },   /* HE160 MCS0 */
	{ 14, 144100 },  /* HE160 MCS1 */
	{ 18, 216200 },  /* HE160 MCS2 */
	{ 20, 288200 },  /* HE160 MCS3 */
	{ 24, 432400 },  /* HE160 MCS4 */
	{ 27, 576500 },  /* HE160 MCS5 */
	{ 29, 648500 },  /* HE160 MCS6 */
	{ 34, 720600 },  /* HE160 MCS7 */
	{ 38, 864700 },  /* HE160 MCS8 */
	{ 40, 960800 },  /* HE160 MCS9 */
	{ 43, 1080900 }, /* HE160 MCS10 */
	{ 45, 1201000 }, /* HE160 MCS11 */
	{ 48, 1297100 }, /* EHT160 MCS12 */
	{ 51, 1441200 }, /* EHT160 MCS13 */
	{ -1, 1441200 }  /* SNR > 51 */
};

/* See IEEE P802.11be/D2.0, Table 36-86: EHT-MCSs for 4x996-tone RU, NSS,u = 1
 */
static const struct minsnr_bitrate_entry eht320_table[] = {
	{ 0, 0 },
	{ 14, 144100 },   /* EHT320 MCS0 */
	{ 17, 288200 },   /* EHT320 MCS1 */
	{ 21, 432400 },   /* EHT320 MCS2 */
	{ 23, 576500 },   /* EHT320 MCS3 */
	{ 27, 864700 },   /* EHT320 MCS4 */
	{ 30, 1152900 },  /* EHT320 MCS5 */
	{ 32, 1297100 },  /* EHT320 MCS6 */
	{ 37, 1441200 },  /* EHT320 MCS7 */
	{ 41, 1729400 },  /* EHT320 MCS8 */
	{ 43, 1921500 },  /* EHT320 MCS9 */
	{ 46, 2161800 },  /* EHT320 MCS10 */
	{ 48, 2401900 },  /* EHT320 MCS11 */
	{ 51, 2594100 },  /* EHT320 MCS12 */
	{ 54, 2882400 },  /* EHT320 MCS13 */
	{ -1, 2882400 }   /* SNR > 54 */
};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

#define MAC_ADDR_HASH(_addr) \
	(_addr[0] ^ _addr[1] ^ _addr[2] ^ _addr[3] ^ _addr[4] ^ _addr[5])
#define AP_HASH(_addr) \
	((uint8_t) (MAC_ADDR_HASH(_addr) & (AP_HASH_SIZE - 1)))

#define CALCULATE_SCORE_BY_DEAUTH(prBssDesc, eRoamType) \
	(gasMtkWeightConfig[eRoamType].ucLastDeauthWeight * \
	(prBssDesc->prBlack && prBssDesc->prBlack->fgDeauthLastTime ? 0 : \
	BSS_FULL_SCORE))

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

static uint8_t apsSanityCheckBssDesc(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc, enum ENUM_ROAMING_REASON eRoamReason,
	uint8_t ucBssIndex);

static uint32_t apsGetEstimatedTput(struct ADAPTER *ad, struct BSS_DESC *bss,
	uint8_t bidx);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

struct AP_COLLECTION *apsHashGet(struct ADAPTER *ad,
	uint8_t *addr, uint8_t bidx)
{
	struct AIS_SPECIFIC_BSS_INFO *s = aisGetAisSpecBssInfo(ad, bidx);
	struct AP_COLLECTION *a = NULL;

	a = s->arApHash[AP_HASH(addr)];

	while (a != NULL &&
	       UNEQUAL_MAC_ADDR(a->aucAddr, addr))
		a = a->hnext;
	return a;
}

void apsHashAdd(struct ADAPTER *ad, struct AP_COLLECTION *ap, uint8_t bidx)
{
	struct AIS_SPECIFIC_BSS_INFO *s = aisGetAisSpecBssInfo(ad, bidx);

	ap->hnext = s->arApHash[AP_HASH(ap->aucAddr)];
	s->arApHash[AP_HASH(ap->aucAddr)] = ap;
}

void apsHashDel(struct ADAPTER *ad, struct AP_COLLECTION *ap, uint8_t bidx)
{
	struct AIS_SPECIFIC_BSS_INFO *s = aisGetAisSpecBssInfo(ad, bidx);
	struct AP_COLLECTION *a = NULL;

	a = s->arApHash[AP_HASH(ap->aucAddr)];

	if (a == NULL)
		return;

	if (EQUAL_MAC_ADDR(a->aucAddr, ap->aucAddr)) {
		s->arApHash[AP_HASH(ap->aucAddr)] = a->hnext;
		return;
	}

	while (a->hnext != NULL &&
	       UNEQUAL_MAC_ADDR(a->hnext->aucAddr, ap->aucAddr)) {
		a = a->hnext;
	}
	if (a->hnext != NULL)
		a->hnext = a->hnext->hnext;
	else
		DBGLOG(APS, INFO, "Could not remove AP " MACSTR
			   " from hash table\n", MAC2STR(ap->aucAddr));
}

uint8_t apsCanFormMld(struct ADAPTER *ad,
	struct BSS_DESC *bss, uint8_t bidx)
{
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;

	if (!mldIsMultiLinkEnabled(ad, NETWORK_TYPE_AIS, bidx) ||
	    !aisSecondLinkAvailable(ad, bidx))
		return FALSE;

	if (bss->fgIsEHTPresent == FALSE ||
	    (!prWifiVar->fgDisSecurityCheck &&
	     !rsnIsKeyMgmtForEht(ad, bss, bidx)))
		return FALSE;

	if (bss->rMlInfo.fgValid) {
		bss->rMlInfo.prBlock = aisQueryMldBlockList(ad, bss);
		return TRUE;
	}
#endif
	return FALSE;
}

uint8_t apsBssDescToLink(struct ADAPTER *ad,
	struct AP_COLLECTION *ap, struct BSS_DESC *bss, uint8_t bidx)
{
	return bss->eBand;
}

uint32_t apsAddBssDescToList(struct ADAPTER *ad, struct AP_COLLECTION *ap,
			struct BSS_DESC *bss, uint8_t bidx)
{
	uint8_t aidx = AIS_INDEX(ad, bidx);
	uint8_t l = apsBssDescToLink(ad, ap, bss, bidx);

	if (l >= BAND_NUM)
		return WLAN_STATUS_FAILURE;

	LINK_ENTRY_INITIALIZE(&bss->rLinkEntryEss[aidx]);
	LINK_INSERT_TAIL(&ap->arLinks[l], &bss->rLinkEntryEss[aidx]);
	ap->ucTotalCount++;

	return WLAN_STATUS_SUCCESS;
}

struct AP_COLLECTION *apsAddAp(struct ADAPTER *ad,
	struct BSS_DESC *bss, uint8_t bidx)
{
	struct AIS_SPECIFIC_BSS_INFO *s = aisGetAisSpecBssInfo(ad, bidx);
	struct LINK *ess = &s->rCurEssLink;
	struct AP_COLLECTION *ap;
	uint8_t i;

	ap = kalMemZAlloc(sizeof(*ap), VIR_MEM_TYPE);
	if (!ap) {
		DBGLOG(APS, WARN, "no resource for " MACSTR "\n",
			MAC2STR(bss->aucBSSID));
		return NULL;
	}

	COPY_MAC_ADDR(ap->aucAddr, bss->aucBSSID);

	ap->fgIsMld = apsCanFormMld(ad, bss, bidx);
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	if (ap->fgIsMld)
		COPY_MAC_ADDR(ap->aucAddr, bss->rMlInfo.aucMldAddr);
	ap->prBlock = aisQueryMldBlockList(ad, bss);
#endif

	DBGLOG(APS, TRACE, "Add CAND[%d][" MACSTR "][MLD=%d]\n",
		ap->u4Index, MAC2STR(ap->aucAddr), ap->fgIsMld);

	for (i = 0; i < BAND_NUM; i++)
		LINK_INITIALIZE(&ap->arLinks[i]);

	if (apsAddBssDescToList(ad, ap, bss, bidx)) {
		kalMemFree(ap, VIR_MEM_TYPE, sizeof(*ap));
		return NULL;
	}

	LINK_INSERT_TAIL(ess, &ap->rLinkEntry);
	apsHashAdd(ad, ap, bidx);

	ap->u4Index = ess->u4NumElem - 1;

	return ap;
}

struct AP_COLLECTION *apsGetAp(struct ADAPTER *ad,
	struct BSS_DESC *bss, uint8_t bidx)
{
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	if (apsCanFormMld(ad, bss, bidx))
		return apsHashGet(ad, bss->rMlInfo.aucMldAddr, bidx);
#endif

	return apsHashGet(ad, bss->aucBSSID, bidx);
}

void apsRemoveAp(struct ADAPTER *ad, struct AP_COLLECTION *ap, uint8_t bidx)
{
	DBGLOG(APS, TRACE,
		"Remove CAND[%d][" MACSTR
		"][MLD=%d] LinkNum=%d, TotalCount=%d\n",
		ap->u4Index, MAC2STR(ap->aucAddr), ap->fgIsMld,
		ap->ucLinkNum, ap->ucTotalCount);

	apsHashDel(ad, ap, bidx);
	kalMemFree(ap, VIR_MEM_TYPE, sizeof(*ap));
}

void apsResetEssApList(struct ADAPTER *ad, uint8_t bidx)
{
	struct AIS_SPECIFIC_BSS_INFO *s = aisGetAisSpecBssInfo(ad, bidx);
	struct LINK *ess = &s->rCurEssLink;
	struct AP_COLLECTION *ap;

	while (!LINK_IS_EMPTY(ess)) {
		LINK_REMOVE_HEAD(ess, ap, struct AP_COLLECTION *);
		apsRemoveAp(ad, ap, bidx);
		/* mem is freed, don't use ap after this point */
	}

	kalMemZero(&s->arApHash[0], sizeof(s->arApHash));
	DBGLOG(APS, INFO, "BssIndex:%d reset prCurEssLink done\n", bidx);
}

#if (CFG_EXT_ROAMING == 0) /* Common */
uint8_t apsIsBssQualify(struct ADAPTER *ad, struct BSS_DESC *bss,
	enum ENUM_ROAMING_REASON eRoamReason, uint32_t u4ConnectedApScore,
	uint32_t u4CandidateApScore, uint8_t bidx)
{
	uint16_t delta = 0;

	/* check min rcpi */
	if (bss->ucRCPI < RCPI_FOR_DONT_ROAM) {
		DBGLOG(APS, INFO, MACSTR " low rssi %d\n",
			MAC2STR(bss->aucBSSID),
			RCPI_TO_dBm(bss->ucRCPI));
		return FALSE;
	}

	/* check min score */
	switch (eRoamReason) {
	case ROAMING_REASON_POOR_RCPI:
	case ROAMING_REASON_INACTIVE:
	case ROAMING_REASON_RETRY:
	{
		delta += ROAM_SCORE_DELTA;

		/* Minimum Roam Delta
		 * Absolute score value comparing to current AP
		 */
		if (u4CandidateApScore <
		    u4ConnectedApScore * (100 + delta) / 100) {
			DBGLOG(APS, TRACE, "BSS[" MACSTR
				"] (%d < %d*%d%%) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				100 + delta, eRoamReason);
			return FALSE;
		}

		break;
	}
	case ROAMING_REASON_BEACON_TIMEOUT:
	{
		/* DON'T compare score if roam with BTO */
		break;
	}
	case ROAMING_REASON_SAA_FAIL:
	{
		/* DON'T compare score if roam with emergency */
		break;
	}
	case ROAMING_REASON_BTM:
	{
		/* DON'T compare score if roam with DIS_IMMI_STATE_3 */
		if (aisGetBTMParam(ad, bidx)->ucDisImmiState ==
		    AIS_BTM_DIS_IMMI_STATE_3)
			break;

		if (u4CandidateApScore < u4ConnectedApScore) {
			DBGLOG(APS, TRACE, "BSS[" MACSTR
				"] (%d < %d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				eRoamReason);
			return FALSE;
		}
		break;
	}
	default:
	{
		if (u4CandidateApScore < u4ConnectedApScore) {
			DBGLOG(APS, TRACE, "BSS[" MACSTR
				"] (%d < %d) reason=%d\n",
				MAC2STR(bss->aucBSSID),
				u4CandidateApScore, u4ConnectedApScore,
				eRoamReason);
			return FALSE;
		}
		break;
	}
	}
	return TRUE;
}
#endif

uint16_t apsGetAmsduByte(struct ADAPTER *ad,
	struct BSS_DESC *bss, uint8_t bidx)
{
	uint16_t bssAmsduLen = 0, amsduLen = 0;
#if (CFG_SUPPORT_802_11BE == 1)
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;

	if (bss->fgIsEHTPresent == TRUE &&
	    (prWifiVar->fgDisSecurityCheck ||
	     rsnIsKeyMgmtForEht(ad, bss, bidx))) {
		bssAmsduLen = (bss->u2MaximumMpdu &
			EHT_MAC_CAP_MAX_MPDU_LEN_MASK) & 0xffff;

		if (bssAmsduLen == EHT_MAC_CAP_MAX_MPDU_LEN_8K)
			amsduLen = APS_AMSDU_VHT_HE_EHT_8K;
		else if (bssAmsduLen == EHT_MAC_CAP_MAX_MPDU_LEN_11K)
			amsduLen = APS_AMSDU_VHT_HE_EHT_11K;
		else if (bssAmsduLen == EHT_MAC_CAP_MAX_MPDU_LEN_3K)
			amsduLen = APS_AMSDU_VHT_HE_EHT_3K;
		else {
			DBGLOG(APS, INFO,
			       "Unexpected EHT maximum mpdu length, %d\n",
			       bssAmsduLen);
			amsduLen = APS_AMSDU_VHT_HE_EHT_3K;
		}
		return amsduLen;
	}
#endif

#if (CFG_SUPPORT_WIFI_6G == 1)
	if (bss->eBand == BAND_6G) {
		bssAmsduLen = (bss->u2MaximumMpdu &
			HE_6G_CAP_INFO_MAX_MPDU_LEN_MASK) & 0xffff;

		if (bssAmsduLen == HE_6G_CAP_INFO_MAX_MPDU_LEN_8K)
			amsduLen = APS_AMSDU_VHT_HE_EHT_8K;
		else if (bssAmsduLen == HE_6G_CAP_INFO_MAX_MPDU_LEN_11K)
			amsduLen = APS_AMSDU_VHT_HE_EHT_11K;
		else if (bssAmsduLen == HE_6G_CAP_INFO_MAX_MPDU_LEN_3K)
			amsduLen = APS_AMSDU_VHT_HE_EHT_3K;
		else {
			DBGLOG(APS, INFO,
				"Unexpected HE maximum mpdu length\n");
			amsduLen = APS_AMSDU_VHT_HE_EHT_3K;
		}
		return amsduLen;
	}
#endif

	if (bss->u2MaximumMpdu) {
		bssAmsduLen = (bss->u2MaximumMpdu &
			VHT_CAP_INFO_MAX_MPDU_LEN_MASK) & 0xffff;
		if (bss->fgIsVHTPresent) {
			if (bssAmsduLen == VHT_CAP_INFO_MAX_MPDU_LEN_8K)
				amsduLen = APS_AMSDU_VHT_HE_EHT_8K;
			else if (bssAmsduLen ==
				VHT_CAP_INFO_MAX_MPDU_LEN_11K)
				amsduLen = APS_AMSDU_VHT_HE_EHT_11K;
			else if (bssAmsduLen ==
				VHT_CAP_INFO_MAX_MPDU_LEN_3K)
				amsduLen = APS_AMSDU_VHT_HE_EHT_3K;
			else {
				DBGLOG(APS, INFO,
					"Unexpected VHT maximum mpdu length\n");
				amsduLen = APS_AMSDU_VHT_HE_EHT_3K;
			}
		} else
			amsduLen = APS_AMSDU_HT_8K;
	} else {
		if (bss->fgIsVHTPresent)
			amsduLen = APS_AMSDU_VHT_HE_EHT_3K;
		else
			amsduLen = APS_AMSDU_HT_3K;
	}

	return amsduLen;
}

static unsigned int interpolate_rate(int snr, int snr0, int snr1,
				     int rate0, int rate1)
{
	return rate0 + (snr - snr0) * (rate1 - rate0) / (snr1 - snr0);
}

static unsigned int max_rate(const struct minsnr_bitrate_entry table[],
			     int snr, bool vht)
{
	const struct minsnr_bitrate_entry *prev, *entry = table;

	while ((entry->minsnr != -1) &&
	       (snr >= entry->minsnr) &&
	       (vht || entry - table <= vht_mcs))
		entry++;
	if (entry == table)
		return entry->bitrate;
	prev = entry - 1;
	if (entry->minsnr == -1 || (!vht && entry - table > vht_mcs))
		return prev->bitrate;
	return interpolate_rate(snr, prev->minsnr, entry->minsnr, prev->bitrate,
				entry->bitrate);
}

static unsigned int max_he_eht_rate(const struct minsnr_bitrate_entry table[],
				    int snr, bool eht)
{
	const struct minsnr_bitrate_entry *prev, *entry = table;

	while (entry->minsnr != -1 && snr >= entry->minsnr &&
	       (eht || entry - table <= EHT_MCS))
		entry++;
	if (entry == table)
		return 0;
	prev = entry - 1;
	if (entry->minsnr == -1 || (!eht && entry - table > EHT_MCS))
		return prev->bitrate;
	return interpolate_rate(snr, prev->minsnr, entry->minsnr,
				prev->bitrate, entry->bitrate);
}

uint32_t apsGetMaxRate(struct ADAPTER *ad, struct BSS_DESC *bss, uint8_t bidx)
{
	uint32_t rate = 5400; /* basic rate */
	/* TODO: get noise or snr when scan done */
	int32_t noise = bss->eBand == BAND_2G4 ? -89 : -92;
	int32_t snr = RCPI_TO_dBm(bss->ucRCPI) - noise;
	int32_t bw = MAX_BW_20MHZ, sta_bw = MAX_BW_20MHZ, ap_bw = MAX_BW_20MHZ;
	int32_t	gen = 0, sta_gen = 0, ap_gen = 0;
	struct WIFI_VAR *prWifiVar = &ad->rWifiVar;

	if (prWifiVar->ucStaHt)
		sta_gen = 4;
	if (bss->fgIsHTPresent)
		ap_gen = 4;

	if (prWifiVar->ucStaVht)
		sta_gen = 5;
	if (bss->fgIsVHTPresent)
		ap_gen = 5;

#if (CFG_SUPPORT_802_11AX == 1)
	if (prWifiVar->ucStaVht)
		sta_gen = 6;
	if (bss->fgIsHEPresent)
		ap_gen = 6;
#endif
#if (CFG_SUPPORT_802_11BE == 1)
	if (prWifiVar->ucStaEht)
		sta_gen = 7;
	if (bss->fgIsEHTPresent == TRUE &&
	    (prWifiVar->fgDisSecurityCheck ||
	     rsnIsKeyMgmtForEht(ad, bss, bidx)))
		ap_gen = 7;
#endif

	if (bss->eBand == BAND_2G4)
		sta_bw = prWifiVar->ucSta2gBandwidth;
	if (bss->eBand == BAND_5G)
		sta_bw = prWifiVar->ucSta5gBandwidth;
#if (CFG_SUPPORT_WIFI_6G == 1)
	if (bss->eBand == BAND_6G)
		sta_bw = prWifiVar->ucSta6gBandwidth;
#endif
	ap_bw = rlmGetBssOpBwByChannelWidth(bss->eSco, bss->eChannelWidth);

	/* get min wifi generation & bandwidth */
	gen = KAL_MIN(ap_gen, sta_gen);
	bw = KAL_MIN(ap_bw, sta_bw);

#if (CFG_SUPPORT_802_11BE == 1)
	if (gen == 7) {
		uint32_t boost = 30;

		if (bw == MAX_BW_320_2MHZ || bw == MAX_BW_320_1MHZ)
			rate = max_he_eht_rate(eht320_table, snr, TRUE);
		else if (bw == MAX_BW_160MHZ || bw == MAX_BW_80_80_MHZ)
			rate = max_he_eht_rate(he160_table, snr, TRUE);
		else if (bw == MAX_BW_80MHZ)
			rate = max_he_eht_rate(he80_table, snr, TRUE);
		else if (bw == MAX_BW_40MHZ)
			rate = max_he_eht_rate(he40_table, snr, TRUE);
		else
			rate = max_he_eht_rate(he20_table, snr, TRUE);
		rate += boost;

		goto done;
	}
#endif

#if (CFG_SUPPORT_802_11AX == 1)
	if (gen == 6) {
		uint32_t boost = 20;

		if (bw == MAX_BW_160MHZ || bw == MAX_BW_80_80_MHZ)
			rate = max_he_eht_rate(he160_table, snr, FALSE);
		else if (bw == MAX_BW_80MHZ)
			rate = max_he_eht_rate(he80_table, snr, FALSE);
		else if (bw == MAX_BW_40MHZ)
			rate = max_he_eht_rate(he40_table, snr, FALSE);
		else
			rate = max_he_eht_rate(he20_table, snr, FALSE);
		rate += boost;

		goto done;
	}
#endif

	if (gen == 5) {
		uint32_t boost = 10;

		if (bw == MAX_BW_160MHZ || bw == MAX_BW_80_80_MHZ)
			rate = max_rate(vht160_table, snr, TRUE) + boost;
		else if (bw == MAX_BW_80MHZ)
			rate = max_rate(vht80_table, snr, TRUE) + boost;
		else if (bw == MAX_BW_40MHZ)
			rate = max_rate(vht40_table, snr, TRUE) + boost;
		else
			rate = max_rate(vht20_table, snr, TRUE) + boost;
		rate += boost;

		goto done;
	}

	if (gen == 4) {
		if (bw == MAX_BW_40MHZ)
			rate = max_rate(vht40_table, snr, FALSE);
		else
			rate = max_rate(vht20_table, snr, FALSE);
		goto done;
	}

done:
	return rate;
}

void apsRecordCuInfo(struct ADAPTER *ad, struct BSS_DESC *bss,
	uint8_t bidx)
{
	struct APS_INFO *aps = aisGetApsInfo(ad, bidx);
	uint16_t u2CuOffset = 0;

	if (bss->eBand == BAND_2G4 || bss->eBand == BAND_5G)
		u2CuOffset = bss->ucChannelNum;
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (bss->eBand == BAND_6G)
		u2CuOffset = CU_6G_INDEX_OFFSET + bss->ucChannelNum;
#endif

	aps->arCuInfo[u2CuOffset].eBand = bss->eBand;
	aps->arCuInfo[u2CuOffset].ucTotalCount++;
	aps->arCuInfo[u2CuOffset].ucTotalCu += bss->ucChnlUtilization;
}

uint8_t apsGetCuInfo(struct ADAPTER *ad, struct BSS_DESC *bss, uint8_t bidx)
{
	struct APS_INFO *aps = aisGetApsInfo(ad, bidx);
	uint16_t u2CuOffset = 0;

	if (bss->eBand == BAND_2G4 || bss->eBand == BAND_5G)
		u2CuOffset = bss->ucChannelNum;
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (bss->eBand == BAND_6G)
		u2CuOffset = CU_6G_INDEX_OFFSET + bss->ucChannelNum;
#endif

	return aps->arCuInfo[u2CuOffset].ucTotalCount == 0 ? 0 :
	       aps->arCuInfo[u2CuOffset].ucTotalCu /
	       aps->arCuInfo[u2CuOffset].ucTotalCount;
}

static uint32_t apsGetEstimatedTput(struct ADAPTER *ad, struct BSS_DESC *bss,
	uint8_t bidx)
{
	struct APS_INFO *aps = aisGetApsInfo(ad, bidx);
	uint8_t fgIsGBandCoex = aps->fgIsGBandCoex, ucChannelCuInfo = 0;
	uint16_t amsduByte = apsGetAmsduByte(ad, bss, bidx);
	uint32_t baSize = 32, slot = 0, rcpi = 0, ppduDuration = 5;
	uint32_t airTime = 0, ideal = 0, tput = 0, est = 0;
	int32_t idle = 0, a = 0, b = 0, delta = 5;
	uint8_t *pucIEs = NULL;

	if (aps->ucConsiderEsp) {
		pucIEs = (uint8_t *) &bss->u4EspInfo[ESP_AC_BE];
		baSize = aucBaSizeTranslate[(uint8_t)((pucIEs[0] & 0xE0) >> 5)];
		airTime = pucIEs[1];
		if (bss->fgExistEspOutIE)
			airTime += bss->ucEspOutInfo[ESP_AC_BE];
		else
			airTime += pucIEs[1];
		airTime = airTime >> 1;
		if (pucIEs[2])
			ppduDuration = pucIEs[2];

		/* Unit: kbps */
		ideal = baSize * amsduByte * 8 / ppduDuration;
		rcpi = bss->ucRCPI;
		/* Consider the TxPwr only when the RCPI is sufficiently good */
		if (bss->fgExistTxPwr && bss->cTransmitPwr < 0 && rcpi > 100)
			rcpi = rcpi - (bss->cTransmitPwr > -10 ?
				bss->cTransmitPwr : -10);

		rcpi = rcpi > 220 ? 220 : rcpi;
		/* Adjust RCPI based on simultaneous equation */
		if (rcpi > 100) {
			/* RCPI from 220 to 100, peak(1) to breakpoint
			 * (1 - delta/100), y = ax + b, through 2 points
			 * (220, 1) (100, 1 - delta /100)
			 */
			a = (delta * 60000 / 100) / 120;
			b = 60000 - ((delta * 60000 / 100) * (220 / 120));
		} else if (rcpi >= 50) {
			/* RCPI from 100 to 50, breakpoint(1 - delta /100) to
			 * zero(0) y = ax + b, through 2 points
			 * (100, 1 - delta /100) (50, 0)
			 */
			a = (60000 - (delta * 60000 / 100)) / 50;
			b = (delta * 60000 / 100) - 60000;
		} else {
			/* RCPI less than 50, estimated tput will be zero */
			a = 0;
			b = 0;
		}
		tput = (uint32_t)((uint64_t)ideal *
			(uint64_t)(a * rcpi + b) / 60000);
		est = PERCENTAGE(airTime, 255) * tput / 100;

		if (bss->fgIsRWMValid && bss->u2ReducedWanMetrics < est)
			est = bss->u2ReducedWanMetrics;
	} else {
		if (bss->fgExistBssLoadIE) {
			airTime = 255 - bss->ucChnlUtilization;
		} else {
			ucChannelCuInfo = apsGetCuInfo(ad, bss, bidx);
			if (ucChannelCuInfo) {
				airTime = 255 - ucChannelCuInfo;
			} else {
				slot = scanGetChnlIdleSlot(ad,
					bss->eBand, bss->ucChannelNum);

				/* 90000 ms = 90ms dwell time to micro sec */
				idle = (slot * 9 * 100) / (90000);
				airTime  = KAL_MAX(idle, 50);

				/* nomalized to 0~255 */
				airTime = airTime * 255 / 100;
			}
		}

		tput = apsGetMaxRate(ad, bss, bidx); /* kbps */
		est = PERCENTAGE(airTime, 255) * tput / 100;
	}

#if (CFG_EXT_ROAMING == 1)
	if (fgIsGBandCoex && bss->eBand == BAND_2G4)
		est = (est * ad->rWifiVar.ucRBTCETPW / 100);
#else
	if (fgIsGBandCoex && bss->eBand == BAND_2G4)
		est = (est * WEIGHT_GBAND_COEX_DOWNGRADE / 100);
#endif

	DBGLOG(APS, TRACE, "BSS["MACSTR
		"] EST:%d tput[%d] bw[%d] rssi[%d] CU[%d] airTime[%d] slot[%d] coex[%d] TxPwr[%d] ideal[%d] ba[%d] amsdu[%d] a[%d] b[%d]\n",
		MAC2STR(bss->aucBSSID), est, tput,
		rlmGetBssOpBwByChannelWidth(bss->eSco, bss->eChannelWidth),
		RCPI_TO_dBm(bss->ucRCPI), ucChannelCuInfo, airTime, slot,
		fgIsGBandCoex, bss->cTransmitPwr,
		ideal, baSize, amsduByte, a, b);

	return est;
}

uint16_t apsUpdateEssApList(struct ADAPTER *ad,
	enum ENUM_ROAMING_REASON reason, uint8_t bidx)
{
	struct APS_INFO *aps = aisGetApsInfo(ad, bidx);
	struct AP_COLLECTION *ap;
	struct BSS_DESC *bss = NULL;
	struct LINK *scan_result = &ad->rWifiVar.rScanInfo.rBSSDescList;
	struct CONNECTION_SETTINGS *conn = aisGetConnSettings(ad, bidx);
	uint16_t count = 0;

	kalMemZero(aps->arCuInfo, sizeof(aps->arCuInfo));
	aps->ucConsiderEsp = TRUE;
	DBGLOG(APS, LOUD, "Update apsUpdateEssApList. reason:%u\n", reason);

#if (CFG_EXT_ROAMING == 1)
	if (reason == ROAMING_REASON_BT_COEX)
		aps->fgIsGBandCoex = TRUE;
#endif

	LINK_FOR_EACH_ENTRY(bss, scan_result, rLinkEntry,
		struct BSS_DESC) {
		if (bss->ucChannelNum > 233)
			continue;
		if (!EQUAL_SSID(conn->aucSSID,
			conn->ucSSIDLen,
			bss->aucSSID, bss->ucSSIDLen) ||
			bss->eBSSType != BSS_TYPE_INFRASTRUCTURE)
			continue;

		bss->prBlack = aisQueryBlockList(ad, bss);
#if CFG_SUPPORT_802_11K
		/* update neighbor report entry */
		bss->prNeighbor = aisGetNeighborAPEntry(
			ad, bss, bidx);
#endif

		/* sanity check */
		if (!apsSanityCheckBssDesc(ad, bss, reason, bidx))
			continue;

		/* add bss to ap collection */
		ap = apsGetAp(ad, bss, bidx);
		if (ap) {
			if (!apsAddBssDescToList(ad, ap, bss, bidx))
				count++;
		} else {
			ap = apsAddAp(ad, bss, bidx);
			if (ap)
				count++;
		}

		/* handle cu/esp */
		if (bss->fgExistBssLoadIE)
			apsRecordCuInfo(ad, bss, bidx);
		if (!bss->fgExistEspIE)
			aps->ucConsiderEsp = FALSE;
	}

	DBGLOG(APS, INFO,
		"Find %s in %d BSSes, result %d, Using %s estimated tput\n",
		conn->aucSSID, scan_result->u4NumElem, count,
		aps->ucConsiderEsp ? "ESP" : "LEGACY");
	return count;
}

static enum ROAM_TYPE roamReasonToType(enum ENUM_ROAMING_REASON type)
{
	enum ROAM_TYPE ret = ROAM_TYPE_RCPI;

	if (type >= ROAMING_REASON_NUM)
		return ret;
#if CFG_SUPPORT_ROAMING
	if (type == ROAMING_REASON_TX_ERR)
		ret = ROAM_TYPE_PER;
#endif
	return ret;
}

#if (CFG_EXT_ROAMING == 0) /* Common part */
/* Channel Utilization: weight index will be */
static uint16_t apsCalculateScoreByChnlInfo(
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo, uint8_t ucChannel,
	enum ROAM_TYPE eRoamType)
{
	struct ESS_CHNL_INFO *prEssChnlInfo = &prAisSpecificBssInfo->
		arCurEssChnlInfo[0];
	uint8_t i = 0;
	uint16_t u2Score = 0;
	uint8_t weight = 0;

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

	weight = gasMtkWeightConfig[eRoamType].ucApNumWeight;

	for (; i < prAisSpecificBssInfo->ucCurEssChnlInfoNum; i++) {
		if (ucChannel == prEssChnlInfo[i].ucChannel) {
#if 0	/* currently, we don't take channel utilization into account */
			/* the channel utilization max value is 255.
			 *great utilization means little weight value.
			 * the step of weight value is 2.6
			 */
			u2Score = mtk_weight_config[eRoamType].
				ucChnlUtilWeight * (BSS_FULL_SCORE -
				(prEssChnlInfo[i].ucUtilization * 10 / 26));
#endif
			/* if AP num on this channel is greater than 100,
			 * the weight will be 0.
			 * otherwise, the weight value decrease 1
			 * if AP number increase 1
			 */
			if (prEssChnlInfo[i].ucApNum <= CHNL_BSS_NUM_THRESOLD)
				u2Score += weight *
				(BSS_FULL_SCORE - prEssChnlInfo[i].ucApNum *
					SCORE_PER_AP);
			DBGLOG(APS, TRACE, "channel %d, AP num %d\n",
				ucChannel, prEssChnlInfo[i].ucApNum);
			break;
		}
	}
	return u2Score;
}

static uint16_t apsCalculateScoreByBW(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc, enum ROAM_TYPE eRoamType,
	uint8_t ucBssIndex)
{
	uint16_t u2Score = 0;

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

	switch (prBssDesc->eChannelWidth) {
	case CW_20_40MHZ:
		u2Score = 40;
		break;
	case CW_80MHZ:
		u2Score = 60;
		break;
	case CW_160MHZ:
	case CW_80P80MHZ:
		u2Score = 80;
		break;
	case CW_320_1MHZ:
	case CW_320_2MHZ:
		u2Score = 100;
		break;
	default:
		break;
	}

	return u2Score * gasMtkWeightConfig[eRoamType].ucBandWidthWeight;
}

static uint16_t apsCalculateScoreByBand(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc, int8_t cRssi, enum ROAM_TYPE eRoamType)
{
	uint16_t u2Score = 0;

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

	switch (prBssDesc->eBand) {
	case BAND_2G4:
		u2Score = 0;
		break;
	case BAND_5G:
		if (prAdapter->fgEnable5GBand && cRssi > LOW_RSSI_FOR_5G_BAND)
			u2Score = 80;
		break;
#if (CFG_SUPPORT_WIFI_6G == 1)
	case BAND_6G:
		if (prAdapter->fgIsHwSupport6G && cRssi > LOW_RSSI_FOR_5G_BAND)
			u2Score = BSS_FULL_SCORE;
		break;
#endif
	default:
		break;
	}

	return u2Score * gasMtkWeightConfig[eRoamType].ucBandWeight;
}

static uint16_t apsCalculateScoreByClientCnt(struct BSS_DESC *prBssDesc,
			enum ROAM_TYPE eRoamType)
{
	uint16_t u2Score = 0;
	uint16_t u2StaCnt = 0;
#define BSS_STA_CNT_NORMAL_SCORE 50
#define BSS_STA_CNT_GOOD_THRESOLD 10

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

	DBGLOG(APS, TRACE, "Exist bss load %d, sta cnt %d\n",
			prBssDesc->fgExistBssLoadIE, prBssDesc->u2StaCnt);

	if (!prBssDesc->fgExistBssLoadIE) {
		u2Score = BSS_STA_CNT_NORMAL_SCORE;
		return u2Score *
		gasMtkWeightConfig[eRoamType].ucClientCntWeight;
	}

	u2StaCnt = prBssDesc->u2StaCnt;
	if (u2StaCnt > BSS_STA_CNT_THRESOLD)
		u2Score = 0;
	else if (u2StaCnt < BSS_STA_CNT_GOOD_THRESOLD)
		u2Score = BSS_FULL_SCORE - u2StaCnt;
	else
		u2Score = BSS_STA_CNT_NORMAL_SCORE;

	return u2Score * gasMtkWeightConfig[eRoamType].ucClientCntWeight;
}

static uint16_t apsCalculateScoreByStbc(struct BSS_DESC *prBssDesc,
	enum ROAM_TYPE eRoamType)
{
	uint16_t u2Score = 0;

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

	if (prBssDesc->fgMultiAnttenaAndSTBC)
		u2Score = BSS_FULL_SCORE;

#if (CFG_SUPPORT_WIFI_6G == 1)
	/* assume stbc is supported because 6g AP doesn't carry ht cap */
	if (prBssDesc->eBand == BAND_6G)
		u2Score = BSS_FULL_SCORE;
#endif

	u2Score *= gasMtkWeightConfig[eRoamType].ucStbcWeight;

	return u2Score;
}

static uint16_t apsCalculateScoreByRssi(struct BSS_DESC *prBssDesc,
	enum ROAM_TYPE eRoamType)
{
	uint16_t u2Score = 0;
	int8_t cRssi = RCPI_TO_dBm(prBssDesc->ucRCPI);

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

	if (cRssi >= BEST_RSSI)
		u2Score = 100;
	else if (prBssDesc->eBand == BAND_5G && cRssi >= GOOD_RSSI_FOR_HT_VHT)
		u2Score = 100;
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (prBssDesc->eBand == BAND_6G && cRssi >= GOOD_RSSI_FOR_HT_VHT)
		u2Score = 100;
#endif
	else if (prBssDesc->eBand == BAND_2G4 && cRssi < MINIMUM_RSSI_2G4)
		u2Score = 0;
	else if (prBssDesc->eBand == BAND_5G && cRssi < MINIMUM_RSSI_5G)
		u2Score = 0;
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (prBssDesc->eBand == BAND_6G && cRssi < MINIMUM_RSSI_6G)
		u2Score = 0;
#endif
	else if (cRssi <= -98)
		u2Score = 0;
	else
		u2Score = (cRssi + 98) * 2;

	u2Score *= gasMtkWeightConfig[eRoamType].ucRssiWeight;

	return u2Score;
}

static uint16_t apsCalculateScoreBySaa(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc, enum ROAM_TYPE eRoamType)
{
	uint16_t u2Score = 0;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

	prStaRec = cnmGetStaRecByAddress(prAdapter, NETWORK_TYPE_AIS,
		prBssDesc->aucSrcAddr);
	if (prStaRec)
		u2Score = gasMtkWeightConfig[eRoamType].ucSaaWeight *
		(prStaRec->ucTxAuthAssocRetryCount ? 0 : BSS_FULL_SCORE);
	else
		u2Score = gasMtkWeightConfig[eRoamType].ucSaaWeight *
		BSS_FULL_SCORE;

	return u2Score;
}

static uint16_t apsCalculateScoreByIdleTime(struct ADAPTER *prAdapter,
	uint8_t ucChannel, enum ROAM_TYPE eRoamType,
	struct BSS_DESC *prBssDesc, uint8_t ucBssIndex,
	enum ENUM_BAND eBand)
{
	struct SCAN_INFO *info;
	struct SCAN_PARAM *param;
	struct BSS_INFO *bss;
	int32_t score, rssi, cu = 0, cuRatio, dwell;
	uint32_t rssiFactor, cuFactor, rssiWeight, cuWeight;
	uint32_t slot = 0, idle;
	uint8_t i;

	rssi = RCPI_TO_dBm(prBssDesc->ucRCPI);
	rssiWeight = 65;
	cuWeight = 35;
	if (rssi >= -55)
		rssiFactor = 100;
	else if (rssi < -55 && rssi >= -60)
		rssiFactor = 90 + 2 * (60 + rssi);
	else if (rssi < -60 && rssi >= -70)
		rssiFactor = 60 + 3 * (70 + rssi);
	else if (rssi < -70 && rssi >= -80)
		rssiFactor = 20 + 4 * (80 + rssi);
	else if (rssi < -80 && rssi >= -90)
		rssiFactor = 2 * (90 + rssi);
	else
		rssiFactor = 0;
	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}
	if (prBssDesc->eBand >= BAND_NUM) {
		DBGLOG(APS, WARN, "Invalid Band %d\n", prBssDesc->eBand);
		return 0;
	}
	if (prBssDesc->fgExistBssLoadIE) {
		cu = prBssDesc->ucChnlUtilization;
	} else {
		bss = aisGetAisBssInfo(prAdapter, ucBssIndex);
		info = &(prAdapter->rWifiVar.rScanInfo);
		param = &(info->rScanParam);

		if (param->u2ChannelDwellTime > 0)
			dwell = param->u2ChannelDwellTime;
		else if (bss->eConnectionState == MEDIA_STATE_CONNECTED)
			dwell = CHNL_DWELL_TIME_ONLINE;
		else
			dwell = CHNL_DWELL_TIME_DEFAULT;

		for (i = 0; i < info->ucSparseChannelArrayValidNum; i++) {
			if (prBssDesc->ucChannelNum == info->aucChannelNum[i] &&
					eBand == info->aeChannelBand[i]) {
				slot = info->au2ChannelIdleTime[i];
				idle = (slot * 9 * 100) / (dwell * 1000);
#if CFG_SUPPORT_ROAMING
				if (eRoamType == ROAM_TYPE_PER) {
					score = idle > BSS_FULL_SCORE ?
						BSS_FULL_SCORE : idle;
					goto done;
				}
#endif
				cu = 255 - idle * 255 / 100;
				break;
			}
		}
	}

	cuRatio = cu * 100 / 255;
	if (prBssDesc->eBand == BAND_2G4) {
		if (cuRatio < 10)
			cuFactor = 100;
		else if (cuRatio < 70 && cuRatio >= 10)
			cuFactor = 111 - (13 * cuRatio / 10);
		else
			cuFactor = 20;
	} else {
		if (cuRatio < 30)
			cuFactor = 100;
		else if (cuRatio < 80 && cuRatio >= 30)
			cuFactor = 148 - (16 * cuRatio / 10);
		else
			cuFactor = 20;
	}

	score = (rssiFactor * rssiWeight + cuFactor * cuWeight) >> 6;

	DBGLOG(APS, TRACE,
		MACSTR
		" Band[%s],chl[%d],slt[%d],ld[%d] idle Score %d,rssi[%d],cu[%d],cuR[%d],rf[%d],rw[%d],cf[%d],cw[%d]\n",
		MAC2STR(prBssDesc->aucBSSID),
		apucBandStr[prBssDesc->eBand],
		prBssDesc->ucChannelNum, slot,
		prBssDesc->fgExistBssLoadIE, score, rssi, cu, cuRatio,
		rssiFactor, rssiWeight, cuFactor, cuWeight);
#if CFG_SUPPORT_ROAMING
done:
#endif
	return score * gasMtkWeightConfig[eRoamType].ucChnlIdleWeight;

}

uint16_t apsCalculateScoreByBlackList(struct ADAPTER *prAdapter,
	    struct BSS_DESC *prBssDesc, enum ROAM_TYPE eRoamType)
{
	uint16_t u2Score = 0;

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

	if (!prBssDesc->prBlack)
		u2Score = 100;
	else if (rsnApOverload(prBssDesc->prBlack->u2AuthStatus,
		prBssDesc->prBlack->u2DeauthReason) ||
		 prBssDesc->prBlack->ucCount >= 10)
		u2Score = 0;
	else
		u2Score = 100 - prBssDesc->prBlack->ucCount * 10;

	return u2Score * gasMtkWeightConfig[eRoamType].ucBlockListWeight;
}

uint16_t apsCalculateScoreByTput(struct ADAPTER *prAdapter,
	    struct BSS_DESC *prBssDesc, enum ROAM_TYPE eRoamType)
{
	uint16_t u2Score = 0;

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

#if CFG_SUPPORT_MBO
	if (prBssDesc->fgExistEspIE)
		u2Score = (prBssDesc->u4EspInfo[ESP_AC_BE] >> 8) & 0xff;
#endif

	return u2Score * gasMtkWeightConfig[eRoamType].ucTputWeight;
}

uint16_t apsCalculateScoreByPreference(struct ADAPTER *prAdapter,
	    struct BSS_DESC *prBssDesc, enum ENUM_ROAMING_REASON eRoamReason)
{
	enum ROAM_TYPE eRoamType = roamReasonToType(eRoamReason);

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}

#if CFG_SUPPORT_ROAMING
#if CFG_SUPPORT_802_11K
	if (prBssDesc->prNeighbor)
		return (prBssDesc->prNeighbor->ucPreference + 100) *
		       gasMtkWeightConfig[eRoamType].ucPreferenceWeight;
#endif
#endif
	return 100 * gasMtkWeightConfig[eRoamType].ucPreferenceWeight;
}

uint16_t apsCalculateApScore(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc, enum ENUM_ROAMING_REASON eRoamReason,
	uint8_t ucBssIndex)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecificBssInfo = NULL;
	struct APS_INFO *aps = aisGetApsInfo(prAdapter, ucBssIndex);
	uint8_t fgIsGBandCoex = aps->fgIsGBandCoex;
	uint16_t u2ScoreStaCnt = 0;
	uint16_t u2ScoreBandwidth = 0;
	uint16_t u2ScoreSTBC = 0;
	uint16_t u2ScoreChnlInfo = 0;
	uint16_t u2ScoreSnrRssi = 0;
	uint16_t u2ScoreDeauth = 0;
	uint16_t u2ScoreBand = 0;
	uint16_t u2ScoreSaa = 0;
	uint16_t u2ScoreIdleTime = 0;
	uint16_t u2ScoreTotal = 0;
	uint16_t u2BlackListScore = 0;
	uint16_t u2PreferenceScore = 0;
	uint16_t u2TputScore = 0;
#if (CFG_SUPPORT_AVOID_DESENSE == 1)
	uint8_t fgBssInDenseRange =
		IS_CHANNEL_IN_DESENSE_RANGE(prAdapter,
		prBssDesc->ucChannelNum,
		prBssDesc->eBand);
	char extra[16] = {0};
#else
	char *extra = "";
#endif
	int8_t cRssi = -128;
	enum ROAM_TYPE eRoamType = roamReasonToType(eRoamReason);

	prAisSpecificBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	cRssi = RCPI_TO_dBm(prBssDesc->ucRCPI);

	if (eRoamType >= ROAM_TYPE_NUM) {
		DBGLOG(APS, WARN, "Invalid roam type %d!\n", eRoamType);
		return 0;
	}
	if (prBssDesc->eBand >= BAND_NUM) {
		DBGLOG(APS, WARN, "Invalid Band %d\n", prBssDesc->eBand);
		return 0;
	}
	u2ScoreBandwidth = apsCalculateScoreByBW(prAdapter,
		prBssDesc, eRoamType, ucBssIndex);
	u2ScoreStaCnt = apsCalculateScoreByClientCnt(prBssDesc, eRoamType);
	u2ScoreSTBC = apsCalculateScoreByStbc(prBssDesc, eRoamType);
	u2ScoreChnlInfo = apsCalculateScoreByChnlInfo(prAisSpecificBssInfo,
				prBssDesc->ucChannelNum, eRoamType);
	u2ScoreSnrRssi = apsCalculateScoreByRssi(prBssDesc, eRoamType);
	u2ScoreDeauth = CALCULATE_SCORE_BY_DEAUTH(prBssDesc, eRoamType);
	u2ScoreBand = apsCalculateScoreByBand(prAdapter, prBssDesc,
		cRssi, eRoamType);
	u2ScoreSaa = apsCalculateScoreBySaa(prAdapter, prBssDesc, eRoamType);
	u2ScoreIdleTime = apsCalculateScoreByIdleTime(prAdapter,
		prBssDesc->ucChannelNum, eRoamType, prBssDesc, ucBssIndex,
		prBssDesc->eBand);
	u2BlackListScore =
	       apsCalculateScoreByBlackList(prAdapter, prBssDesc, eRoamType);
	u2PreferenceScore =
	      apsCalculateScoreByPreference(prAdapter, prBssDesc, eRoamReason);

	u2TputScore = apsCalculateScoreByTput(prAdapter, prBssDesc, eRoamType);

	u2ScoreTotal = u2ScoreBandwidth + u2ScoreChnlInfo +
		u2ScoreDeauth + u2ScoreSnrRssi + u2ScoreStaCnt + u2ScoreSTBC +
		u2ScoreBand + u2BlackListScore + u2ScoreSaa +
		u2ScoreIdleTime + u2TputScore;

	/* Adjust 2.4G AP's score if BT coex */
	if (prBssDesc->eBand == BAND_2G4 && fgIsGBandCoex)
		u2ScoreTotal = u2ScoreTotal * WEIGHT_GBAND_COEX_DOWNGRADE / 100;

#if (CFG_SUPPORT_AVOID_DESENSE == 1)
	if (fgBssInDenseRange)
		u2ScoreTotal /= 4;
	kalSnprintf(extra, sizeof(extra), ", DESENSE[%d]", fgBssInDenseRange);
#endif

#define TEMP_LOG_TEMPLATE\
		"BSS["MACSTR"] Score:%d Band[%s],cRSSI[%d],DE[%d]"\
		",RSSI[%d],GBandCoex[%d],BD[%d],BL[%d],SAA[%d]"\
		",BW[%d],SC[%d],ST[%d],CI[%d],IT[%d],CU[%d,%d],PF[%d]"\
		",TPUT[%d]%s\n"

	DBGLOG(APS, TRACE,
		TEMP_LOG_TEMPLATE,
		MAC2STR(prBssDesc->aucBSSID),
		u2ScoreTotal, apucBandStr[prBssDesc->eBand],
		cRssi, fgIsGBandCoex, u2ScoreDeauth,
		u2ScoreSnrRssi, u2ScoreBand, u2BlackListScore,
		u2ScoreSaa, u2ScoreBandwidth, u2ScoreStaCnt,
		u2ScoreSTBC, u2ScoreChnlInfo, u2ScoreIdleTime,
		prBssDesc->fgExistBssLoadIE,
		prBssDesc->ucChnlUtilization,
		u2PreferenceScore,
		u2TputScore, extra);

#undef TEMP_LOG_TEMPLATE

	return u2ScoreTotal;
}
#endif /* CFG_EXT_ROAMING  */

#if (CFG_EXT_ROAMING == 1) && (CFG_SUPPORT_NCHO == 1)
#if (CFG_SUPPORT_802_11BE_MLO == 1)
uint32_t apsGetMloLinkNum(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	uint32_t ucMloLinkNum = 0;
	struct MLD_BSS_INFO *prMldBssInfo;

	prMldBssInfo = aisGetMldBssInfo(prAdapter, ucBssIndex);
	if (!prMldBssInfo) {
		DBGLOG(APS, WARN, "prMldBssInfo doesn't exist!\n");
		return ucMloLinkNum;
	}

	ucMloLinkNum = prMldBssInfo->rBssList.u4NumElem;

	DBGLOG(APS, LOUD, "AP MLD link counter = %d\n", ucMloLinkNum);
	return ucMloLinkNum;
}
#endif /* CFG_SUPPORT_802_11BE_MLO */
#endif
uint8_t apsSanityCheckBssDesc(struct ADAPTER *prAdapter,
	struct BSS_DESC *prBssDesc, enum ENUM_ROAMING_REASON eRoamReason,
	uint8_t ucBssIndex)
{
	struct AIS_FSM_INFO *ais;
	struct BSS_INFO *prAisBssInfo;
	struct CONNECTION_SETTINGS *conn;
	enum ENUM_PARAM_CONNECTION_POLICY policy;
#if CFG_SUPPORT_MBO
	struct PARAM_BSS_DISALLOWED_LIST *disallow;
	struct BSS_TRANSITION_MGT_PARAM *prBtmParam;
	uint32_t i;
#endif

	ais = aisGetAisFsmInfo(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	conn = aisGetConnSettings(prAdapter, ucBssIndex);
	policy = conn->eConnectionPolicy;

	if (!prBssDesc->fgIsInUse) {
		DBGLOG(APS, WARN, MACSTR" is not in use\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}

	if ((prBssDesc->eBand == BAND_2G4 &&
		prAdapter->rWifiVar.ucDisallowBand2G) ||
	    (prBssDesc->eBand == BAND_5G &&
		prAdapter->rWifiVar.ucDisallowBand5G)
#if (CFG_SUPPORT_WIFI_6G == 1)
	 || (prBssDesc->eBand == BAND_6G &&
		prAdapter->rWifiVar.ucDisallowBand6G)
#endif
	) {
		DBGLOG(APS, WARN, MACSTR" Band[%s] is not allowed\n",
			MAC2STR(prBssDesc->aucBSSID),
			apucBandStr[prBssDesc->eBand]);
		return FALSE;
	}

#if (CFG_SUPPORT_802_11BE_MLO == 1)
	if (prBssDesc->rMlInfo.fgValid &&
		!(BIT(prBssDesc->rMlInfo.ucLinkIndex) & conn->u2LinkIdBitmap)) {
		DBGLOG(APS, WARN, MACSTR" LinkID[%d] is not allowed [%d]\n",
			MAC2STR(prBssDesc->aucBSSID),
			prBssDesc->rMlInfo.ucLinkIndex,
			conn->u2LinkIdBitmap);
		return FALSE;
	}

	if (ais->ucMlProbeEnable &&
	    (!prBssDesc->rMlInfo.fgValid ||
	     UNEQUAL_MAC_ADDR(prBssDesc->rMlInfo.aucMldAddr,
			      ais->prMlProbeBssDesc->rMlInfo.aucMldAddr))) {
		DBGLOG(APS, WARN, MACSTR" is not the target of MLO scan.\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}

	if (mldIsMultiLinkEnabled(prAdapter, NETWORK_TYPE_AIS, ucBssIndex) &&
	    prBssDesc->rMlInfo.u2ApRemovalTimer) {
		DBGLOG(APS, WARN, MACSTR " is being removed.\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}
#endif

	if (prBssDesc->eBSSType != BSS_TYPE_INFRASTRUCTURE) {
		DBGLOG(APS, WARN, MACSTR" is not infrastructure\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}

	if (prBssDesc->prBlack) {
		if (prBssDesc->prBlack->fgIsInFWKBlacklist) {
			DBGLOG(APS, WARN, MACSTR" in FWK blocklist\n",
				MAC2STR(prBssDesc->aucBSSID));
			return FALSE;
		}

		if (prBssDesc->prBlack->fgDeauthLastTime) {
			DBGLOG(APS, WARN, MACSTR " is sending deauth.\n",
				MAC2STR(prBssDesc->aucBSSID));
			return FALSE;
		}

		if (prBssDesc->prBlack->ucCount >= AIS_BSS_JOIN_FAIL_LIMIT)  {
			DBGLOG(APS, WARN,
				MACSTR
				" Skip AP that add to blocklist count >= %d\n",
				MAC2STR(prBssDesc->aucBSSID),
				AIS_BSS_JOIN_FAIL_LIMIT);
#if (CFG_SUPPORT_CONN_LOG == 1)
				connLogConnectFail(prAdapter, ucBssIndex,
					CONN_FAIL_BLACLIST_LIMIT);
#endif
			return FALSE;
		}
	}

	/* concurrent STA */
	if ((prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED ||
	    aisFsmIsInProcessPostpone(prAdapter, ucBssIndex))) {
		/* onle limited for wlan1 */
		if (ais->ucAisIndex != AIS_DEFAULT_INDEX) {
			struct AIS_FSM_INFO *mainAis =
				aisFsmGetInstance(prAdapter, AIS_DEFAULT_INDEX);
			struct BSS_DESC *mainBssDesc =
				aisGetMainLinkBssDesc(mainAis);

			/* Disallow to pick a bss that already connected */
			if (IS_AIS_CONN_BSSDESC(mainAis, mainBssDesc)) {
				DBGLOG(APS, INFO,
					MACSTR " already connected by wlan0",
					MAC2STR(prBssDesc->aucBSSID));
				return FALSE;
			}

			/* Disallow wlan1 to use same band with wlan0 */
			if (
#if (CFG_SUPPORT_802_11BE_MLO == 1)
			    /* When mainAis is MLO, always DBDC */
			    mldGetMloLinkNum(prAdapter,
				aisGetMainLinkStaRec(mainAis)) <= 1 &&
#endif
			    mainBssDesc &&
			    prBssDesc->eBand == mainBssDesc->eBand) {
				DBGLOG(APS, INFO,
					MACSTR " same band with wlan0",
					MAC2STR(prBssDesc->aucBSSID));
				return FALSE;
			}
		}
	}

#if CFG_SUPPORT_NCHO
	if (prAdapter->rNchoInfo.fgNCHOEnabled) {
		if (!(BIT(prBssDesc->eBand) &
			prAdapter->rNchoInfo.ucRoamBand)) {
			DBGLOG(APS, WARN,
				MACSTR" band(%s) is not in NCHO roam band\n",
				MAC2STR(prBssDesc->aucBSSID),
				apucBandStr[prBssDesc->eBand]);
			return FALSE;
		}

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		/*  1. check if AIS is currently connected to
		 *  multilink AP-MLD
		 */
		if (apsGetMloLinkNum(prAdapter, ucBssIndex) > 1) {
			/* 2. check candidate is affiliated link of MLO */
			if (apsCanFormMld(prAdapter, prBssDesc, ucBssIndex)) {
				struct BSS_DESC *bss = NULL;
				struct LINK *scan_result =
				    &prAdapter->rWifiVar.rScanInfo.rBSSDescList;
				uint8_t highestband = BAND_NULL;

				/*  3. get the highest band of
				 *  candidate's AP-MLD
				 */
				LINK_FOR_EACH_ENTRY(bss, scan_result,
					rLinkEntry, struct BSS_DESC) {
					if (EQUAL_MAC_ADDR(bss->
						rMlInfo.aucMldAddr,
					    prBssDesc->rMlInfo.aucMldAddr) &&
					    bss->eBand > highestband) {
						highestband = bss->eBand;
					}
				}
				/*
				 *  DBGLOG(APS, WARN, "highestband of AP-Mld["
				 *       MACSTR"] : %u\n",
				 *       MAC2STR(prBssDesc->rMlInfo.aucMldAddr),
				 *       highestband);
				 */

				/*  4. return FALSE if this candidate is not
				 *  the highest band among links in
				 *  the target AP-MLD
				 */
				if (prBssDesc->eBand != highestband) {
				/*
				 * DBGLOG(APS, WARN, "SSID["MACSTR
				 * "] is AP-MLD link. and "
				 * "it's eBand[%u]"
				 * "is not highestband : %u"
				 * "-> filter out !!\n",
				 * MAC2STR(prBssDesc->aucBSSID),
				 * prBssDesc->eBand, highestband);
				 */
					return FALSE;
				}

			/* if candidate is non MLO */
			} else {
				uint8_t curr_highestband = BAND_NULL;
				struct BSS_INFO *prBssInfo;
				struct MLD_BSS_INFO *prMldBssInfo;
				struct LINK *prBssList;
				int32_t curr_rssi;

				prMldBssInfo = aisGetMldBssInfo(prAdapter,
								ucBssIndex);
				prBssList = &prMldBssInfo->rBssList;
				LINK_FOR_EACH_ENTRY(prBssInfo, prBssList,
					rLinkEntryMld, struct BSS_INFO) {
					if (prBssInfo->eBand > curr_highestband)
						curr_highestband =
							prBssInfo->eBand;
					if (prBssDesc->eBand ==
							prBssInfo->eBand)
						curr_rssi =
							RCPI_TO_dBm(prAdapter->
							    rLinkQuality.
							    rLq[prBssInfo->
							    ucBssIndex].cRssi);
				}

				if (prBssDesc->eBand < curr_highestband &&
				    (eRoamReason == ROAMING_REASON_POOR_RCPI ||
				     eRoamReason == ROAMING_REASON_RETRY) &&
				    (curr_rssi >
					prAdapter->rNchoInfo.i4RoamTrigger ||
				     curr_rssi +
					prAdapter->rNchoInfo.i4RoamDelta >
					RCPI_TO_dBm(prBssDesc->ucRCPI))) {
				/* DBGLOG(APS, WARN, "NON MLO AP SSID["MACSTR \
				 * "]'s eBand[%u] has lower RSSI:[%d] than" \
				 * " current eBand[%u]'s RSSI[%d] " \
				 * "-> filter out !!\n",
				 * MAC2STR(prBssDesc->aucBSSID),
				 * prBssDesc->eBand,
				 * RCPI_TO_dBm(prBssDesc->ucRCPI),
				 * overlapped_band, curr_rssi);
				 */
					return FALSE;
				}
			}
		} else
			DBGLOG(APS, WARN, "current AP is single link\n");
#endif
	}
#endif

	if (!(prBssDesc->ucPhyTypeSet &
		(prAdapter->rWifiVar.ucAvailablePhyTypeSet))) {
		DBGLOG(APS, WARN,
			MACSTR" ignore unsupported ucPhyTypeSet = %x\n",
			MAC2STR(prBssDesc->aucBSSID),
			prBssDesc->ucPhyTypeSet);
		return FALSE;
	}

	if (prBssDesc->fgIsUnknownBssBasicRate) {
		DBGLOG(APS, WARN, MACSTR" unknown bss basic rate\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}

	if (!rlmDomainIsLegalChannel(prAdapter, prBssDesc->eBand,
		prBssDesc->ucChannelNum)) {
		DBGLOG(APS, WARN, MACSTR" band %d channel %d is not legal\n",
			MAC2STR(prBssDesc->aucBSSID), prBssDesc->eBand,
			prBssDesc->ucChannelNum);
		return FALSE;
	}

	if (CHECK_FOR_TIMEOUT(kalGetTimeTick(), prBssDesc->rUpdateTime,
		SEC_TO_SYSTIME(wlanWfdEnabled(prAdapter) ?
		SCN_BSS_DESC_STALE_SEC_WFD : AP_SELECTION_DESC_STALE_SEC))) {
		DBGLOG(APS, WARN, MACSTR " description is too old.\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}

	/* BTO case */
	if (prBssDesc->fgIsInBTO) {
		DBGLOG(APS, WARN, MACSTR " is in BTO.\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}

	if (!rsnPerformPolicySelection(prAdapter, prBssDesc,
		ucBssIndex)) {
		DBGLOG(APS, WARN, MACSTR " rsn policy select fail.\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}

	if (aisGetAisSpecBssInfo(prAdapter,
		ucBssIndex)->fgCounterMeasure) {
		DBGLOG(APS, WARN, MACSTR " Skip in counter measure period.\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}

#if CFG_SUPPORT_MBO
	prBtmParam = aisGetBTMParam(prAdapter, ucBssIndex);
	disallow = &prAdapter->rWifiVar.rBssDisallowedList;
	for (i = 0; i < disallow->u4NumBssDisallowed; ++i) {
		uint32_t index = i * MAC_ADDR_LEN;

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID,
				&disallow->aucList[index])) {
			DBGLOG(APS, WARN, MACSTR" disallowed list\n",
				MAC2STR(prBssDesc->aucBSSID));
			return FALSE;
		}
	}

	if (prBssDesc->fgIsDisallowed) {
		DBGLOG(APS, WARN, MACSTR" disallowed\n",
			MAC2STR(prBssDesc->aucBSSID));
		return FALSE;
	}

	if (prBssDesc->prBlack && prBssDesc->prBlack->fgDisallowed &&
	    (eRoamReason != ROAMING_REASON_BTM ||
	     prBtmParam->ucDisImmiState == AIS_BTM_DIS_IMMI_STATE_3)) {
		DBGLOG(APS, WARN, MACSTR" disallowed delay, rssi %d(%d)\n",
			MAC2STR(prBssDesc->aucBSSID),
			RCPI_TO_dBm(prBssDesc->ucRCPI),
			prBssDesc->prBlack->i4RssiThreshold);
		return FALSE;
	}

	if (eRoamReason == ROAMING_REASON_BTM) {
		if (aisCheckNeighborApValidity(prAdapter, ucBssIndex)) {
			uint8_t ucRequestMode = prBtmParam->ucRequestMode;

			if (prBssDesc->prNeighbor &&
			    prBssDesc->prNeighbor->fgPrefPresence &&
			    !prBssDesc->prNeighbor->ucPreference) {
				DBGLOG(APS, WARN,
				     MACSTR " preference is 0, skip it\n",
				     MAC2STR(prBssDesc->aucBSSID));
				return FALSE;
			}

			if ((ucRequestMode & WNM_BSS_TM_REQ_ABRIDGED) &&
			    !prBssDesc->prNeighbor &&
			    !IS_AIS_CONN_BSSDESC(ais, prBssDesc)) {
				DBGLOG(APS, WARN,
				     MACSTR " not in candidate list, skip it\n",
				     MAC2STR(prBssDesc->aucBSSID));
				return FALSE;
			}
		}
	}
#endif

	return TRUE;
}

uint8_t apsIntraNeedReplace(struct ADAPTER *ad,
	struct BSS_DESC *cand, struct BSS_DESC *curr,
	uint16_t cand_score, uint16_t curr_score,
	enum ENUM_ROAMING_REASON reason, uint8_t bidx)
{
	struct AIS_FSM_INFO *ais = aisGetAisFsmInfo(ad, bidx);

	if (!cand && curr && IS_AIS_CONN_BSSDESC(ais, curr))
		return TRUE;

	if (curr_score > cand_score)
		return TRUE;

	return FALSE;
}

uint8_t apsLinkPlanDecision(struct ADAPTER *prAdapter,
	struct AP_COLLECTION *prAp, enum ENUM_MLO_LINK_PLAN eLinkPlan,
	uint8_t ucBssIndex)
{
	uint32_t u4LinkPlanBmap = BIT(MLO_LINK_PLAN_2_5);

#if (CFG_SUPPORT_WIFI_6G == 1)
#if (CFG_MLO_LINK_PLAN_MODE == 0)
	u4LinkPlanBmap |= BIT(MLO_LINK_PLAN_2_6); /* 2+5/6 */
#else
	u4LinkPlanBmap |= BIT(MLO_LINK_PLAN_2_5_6); /* 2+5+6 */
#endif
#endif

	return !!(u4LinkPlanBmap & BIT(eLinkPlan));
}

struct BSS_DESC *apsIntraUpdateCandi(struct ADAPTER *ad,
	struct AP_COLLECTION *ap, enum ENUM_BAND eBand, uint16_t min_score,
	enum ENUM_ROAMING_REASON reason, uint8_t ignore_policy, uint8_t bidx)
{
	struct AIS_FSM_INFO *ais = aisGetAisFsmInfo(ad, bidx);
	struct CONNECTION_SETTINGS *conn = aisGetConnSettings(ad, bidx);
	enum ENUM_PARAM_CONNECTION_POLICY policy = conn->eConnectionPolicy;
	struct LINK *link = &ap->arLinks[eBand];
	uint8_t aidx = AIS_INDEX(ad, bidx);
	struct BSS_DESC *bss, *cand = NULL;
	uint16_t score, goal_score = 0;
	uint8_t search_blk = ignore_policy;

try_again:
	LINK_FOR_EACH_ENTRY(bss, link, rLinkEntryEss[aidx], struct BSS_DESC) {
		/*
		 * Skip if
		 * 1. bssid is in driver's blacklist in the first round
		 * 2. already picked
		 */
		if ((!search_blk && bss->prBlack) || bss->fgPicked)
			continue;

		if (!ignore_policy) {
			if (policy == CONNECT_BY_BSSID) {
				if (bss->fgIsMatchBssid) {
					cand = bss;
					break;
				}
				continue;
			} else if (policy == CONNECT_BY_BSSID_HINT) {
				if (bss->fgIsMatchBssidHint) {
					cand = bss;
					break;
				}
				/* didn't conn/scan bssid_hint yet */
				if (ais->ucConnTrialCount == 0)
					continue;
			}

			/* Skip connected AP */
			if (reason != ROAMING_REASON_UPPER_LAYER_TRIGGER &&
			    reason != ROAMING_REASON_BEACON_TIMEOUT &&
			    IS_AIS_CONN_BSSDESC(ais, bss)) {
				DBGLOG(APS, WARN, MACSTR" connected\n",
					MAC2STR(bss->aucBSSID));
				continue;
			}

			/* Skip generated AP */
			if (bss->fgDriverGen) {
				DBGLOG(APS, WARN, "BSS[" MACSTR
					"] is driver gen\n",
					MAC2STR(bss->aucBSSID));
				continue;
			}

			if (!apsIsBssQualify(ad, bss, reason, min_score,
				bss->u2Score, bidx))
				continue;
		}

		score = bss->u2Score;
		if (apsIntraNeedReplace(ad, cand, bss,
			goal_score, score, reason, bidx)) {
			cand = bss;
			goal_score = score;
		}
	}

	if (cand) {
		if (IS_AIS_CONN_BSSDESC(ais, cand) &&
		    !search_blk && link->u4NumElem > 1) {
			search_blk = TRUE;
			goto try_again;
		}

		cand->fgPicked = TRUE;
		goto done;
	}

	/* if No Candidate BSS is found, try BSSes which are in blocklist */
	if (!search_blk && link->u4NumElem > 1) {
		search_blk = TRUE;
		goto try_again;
	}
done:
	return cand;
}

void apsUpdateTotalScore(struct ADAPTER *ad,
	struct BSS_DESC *links[], uint8_t link_num,
	enum ENUM_MLO_LINK_PLAN curr_plan,
	struct AP_COLLECTION *ap, uint8_t bidx)
{
	uint32_t total_score = 0;
	uint32_t total_tput = 0;
	uint8_t fgIsMatchBssid = FALSE;
	uint8_t fgIsMatchBssidHint = FALSE;
	uint8_t i;

#if (CFG_EXT_ROAMING == 0)
	for (i = 0; i < link_num; i++) {
		total_score += links[i]->u2Score;
		total_tput += links[i]->u4Tput;

		if (links[i]->fgIsMatchBssid)
			fgIsMatchBssid = TRUE;
		if (links[i]->fgIsMatchBssidHint)
			fgIsMatchBssidHint = TRUE;
	}
#else
	for (i = 0; i < link_num; i++) {
		if (links[i]->fgIsMatchBssid)
			fgIsMatchBssid = TRUE;
		if (links[i]->fgIsMatchBssidHint)
			fgIsMatchBssidHint = TRUE;
	}

	total_score = links[0]->u2Score;
	total_tput = links[0]->u4Tput;

	if (link_num > 1) {
		total_score =
			total_score * (ad->rWifiVar.ucRCMloTpPref + 100) / 100;
		total_tput =
			total_tput * (ad->rWifiVar.ucRCMloTpPref + 100) / 100;
	}
#endif

	if ((!fgIsMatchBssid && ap->fgIsMatchBssid) ||
	    (!fgIsMatchBssidHint && ap->fgIsMatchBssidHint))
		return;

	if ((fgIsMatchBssid && !ap->fgIsMatchBssid) ||
	    (fgIsMatchBssidHint && !ap->fgIsMatchBssidHint) ||
	    (total_tput > ap->u4TotalTput) ||
	    (total_tput == ap->u4TotalTput && link_num > ap->ucLinkNum)) {
		kalMemCopy(ap->aprTarget, links, sizeof(ap->aprTarget));
		ap->ucLinkNum = link_num;
		ap->u4TotalScore = total_score;
		ap->u4TotalTput = total_tput;
		ap->eMloMode = MLO_MODE_STR;
		ap->ucMaxSimuLinks = link_num - 1;
		ap->fgIsMatchBssid = fgIsMatchBssid;
		ap->fgIsMatchBssidHint = fgIsMatchBssidHint;

		DBGLOG(APS, TRACE,
			"CAND[%d] num[%d,%s] score[%d] tput[%d] mode[%d] simu[%d]\n",
			ap->u4Index, ap->ucLinkNum, apuceLinkPlanStr[curr_plan],
			ap->u4TotalScore, ap->u4TotalTput,
			ap->eMloMode, ap->ucMaxSimuLinks);
	}
}

uint32_t apsSortGetScore(struct BSS_DESC *candi)
{
	if (candi) {
		if (candi->fgIsMatchBssid || candi->fgIsMatchBssidHint)
			return UINT_MAX;
		else if (!candi->fgDriverGen)
			return candi->u4Tput;
	}
	return 0;
}

uint8_t apsSortTrimCandiByScore(struct ADAPTER *ad, struct BSS_DESC *candi[],
	enum ENUM_MLO_LINK_PLAN *curr_plan, uint8_t *curr_rfband_bmap)
{
	struct BSS_DESC *bss;
	int i, j;
	uint8_t link_num = 0, band_bmap = 0;

	/* insertion sort by score */
	for (i = 1; i < APS_LINK_MAX; i++) {
		uint32_t tput = 0;

		bss = candi[i];
		tput = apsSortGetScore(bss);

		/* Ensure that NULL will be place at the end */
		for (j = i - 1; j >= 0 && (!candi[j] ||
			apsSortGetScore(candi[j]) < tput); j--)
			candi[j + 1] = candi[j];

		candi[j + 1] = bss;
	}

	/* ensure no null target */
	for (i = 0; i < APS_LINK_MAX; i++) {
		if (candi[i])
			link_num++;
	}

#if (CFG_SUPPORT_802_11BE == 1)
	/* trim ap */
	if (link_num > ad->rWifiVar.ucStaMldLinkMax) {
		DBGLOG(APS, INFO, "trim links %d => %d",
			link_num, ad->rWifiVar.ucStaMldLinkMax);
		link_num = ad->rWifiVar.ucStaMldLinkMax;
	}
#endif

	/* find matched link plan by final link combination */
	for (i = 0; i < link_num; i++)
		band_bmap |= BIT(candi[i]->eBand);
	*curr_plan = apsSearchLinkPlan(ad, band_bmap, link_num);
	*curr_rfband_bmap = band_bmap;

	return link_num;
}

enum ENUM_MLO_LINK_PLAN apsSearchLinkPlan(struct ADAPTER *prAdapter,
	uint8_t ucRfBandBmap, uint8_t ucLinkNum)
{
	switch (ucRfBandBmap) {
	case BIT(BAND_2G4):
		if (ucLinkNum == 1)
			return MLO_LINK_PLAN_2;
		break;
	case BIT(BAND_5G): /* 5 or 5+5 */
		if (ucLinkNum == 1)
			return MLO_LINK_PLAN_5;
		else if (ucLinkNum == 2)
			return MLO_LINK_PLAN_5_5;
		break;
	case BIT(BAND_2G4) | BIT(BAND_5G): /* 2+5 or 2+5+5 */
		if (ucLinkNum == 2)
			return MLO_LINK_PLAN_2_5;
		else if (ucLinkNum == 3)
			return MLO_LINK_PLAN_2_5_5;
		break;
#if (CFG_SUPPORT_WIFI_6G == 1)
	case BIT(BAND_6G): /* 6 or 6+6 */
		if (ucLinkNum == 1)
			return MLO_LINK_PLAN_6;
		else if (ucLinkNum == 2)
			return MLO_LINK_PLAN_6_6;
		break;
	case BIT(BAND_2G4) | BIT(BAND_6G): /* 2+6 or 2+6+6 */
		if (ucLinkNum == 2)
			return MLO_LINK_PLAN_2_6;
		else if (ucLinkNum == 3)
			return MLO_LINK_PLAN_2_6_6;
		break;
	case BIT(BAND_5G) | BIT(BAND_6G):
		if (ucLinkNum == 2)
			return MLO_LINK_PLAN_5_6;
		break;
	case BIT(BAND_2G4) | BIT(BAND_5G) | BIT(BAND_6G):
		if (ucLinkNum == 3)
			return MLO_LINK_PLAN_2_5_6;
		break;
#endif
	default:
		break;
	}

	return MLO_LINK_PLAN_NUM;
}

#if (CFG_SUPPORT_802_11BE_MLO == 1)
uint8_t apsMldBlockListAllow(struct ADAPTER *ad, struct AP_COLLECTION *ap,
	enum ENUM_MLO_LINK_PLAN plan)
{
	if (plan >= MLO_LINK_PLAN_NUM)
		return FALSE;

#if 0
	/* for single link, use legacy block list instead of mld block list */
	if (plan == MLO_LINK_PLAN_2 ||
	    plan == MLO_LINK_PLAN_5
#if (CFG_SUPPORT_WIFI_6G == 1)
	    ||  plan == MLO_LINK_PLAN_6
#endif
	)
		return TRUE;
#endif

	/* disallow if over mld block limit*/
	if (ap->prBlock &&
	    (ap->prBlock->u4BlockBmap & BIT(plan)) &&
	    ap->prBlock->aucCount[plan] >= ad->rWifiVar.ucMldRetryCount)
		return FALSE;

	return TRUE;
}
#endif

void apsIntraSelectLinkPlan(struct ADAPTER *ad, struct AP_COLLECTION *ap,
	uint16_t min_score, uint8_t min_rfband_bmap,
	enum ENUM_ROAMING_REASON reason, uint8_t bidx)
{
	uint8_t aidx = AIS_INDEX(ad, bidx);
	struct CONNECTION_SETTINGS *conn = aisGetConnSettings(ad, bidx);
	enum ENUM_PARAM_CONNECTION_POLICY policy = conn->eConnectionPolicy;
	uint32_t allow_plan_bitmap = 0;
	enum ENUM_BAND *link_plan;
	enum ENUM_MLO_LINK_PLAN curr_plan;
	struct BSS_DESC *bss;
	int i, j;

	/* candi bss scoring */
	for (i = BAND_2G4; i < BAND_NUM; i++) {
		struct LINK *link = &ap->arLinks[i];

		LINK_FOR_EACH_ENTRY(bss, link,
			rLinkEntryEss[aidx], struct BSS_DESC) {

			bss->u2Score =
				apsCalculateApScore(ad, bss, reason, bidx);
			bss->u4Tput =
				apsGetEstimatedTput(ad, bss, bidx);
			bss->fgIsMatchBssid = FALSE;
			bss->fgIsMatchBssidHint = FALSE;

#if (CFG_SUPPORT_ROAMING_LOG == 1)
			roamingFsmLogCandi(ad, bss, reason, bidx);
#endif

			if (policy == CONNECT_BY_BSSID) {
				if (EQUAL_MAC_ADDR(bss->aucBSSID,
						   conn->aucBSSID)) {
					bss->fgIsMatchBssid = TRUE;
					bss->u2Score = BSS_MATCH_BSSID_SCORE;
				}
			} else if (policy == CONNECT_BY_BSSID_HINT) {
				uint8_t oce = FALSE;
				uint8_t chnl = nicFreq2ChannelNum(
						conn->u4FreqInMHz * 1000);

#if CFG_SUPPORT_MBO
				oce = ad->rWifiVar.u4SwTestMode ==
					ENUM_SW_TEST_MODE_SIGMA_OCE;
#endif
				if (!oce && EQUAL_MAC_ADDR(bss->aucBSSID,
					conn->aucBSSIDHint) &&
				    (chnl == 0 || chnl == bss->ucChannelNum)) {
#if (CFG_SUPPORT_AVOID_DESENSE == 1)
					if (IS_CHANNEL_IN_DESENSE_RANGE(
						ad,
						bss->ucChannelNum,
						bss->eBand)) {
						DBGLOG(APS, INFO,
							"Do network selection even match bssid_hint\n");
					} else
#endif
					{
						bss->fgIsMatchBssidHint = TRUE;
						bss->u2Score =
						     BSS_MATCH_BSSID_HINT_SCORE;
					}
				}
			}
		}
	}

	/* list all combination of allowed link plan */
	for (i = 0; i < MLO_LINK_PLAN_NUM; i++) {
		uint8_t allow;

		allow = apsLinkPlanDecision(ad, ap, i, bidx);

#if (CFG_SINGLE_BAND_MLSR_56 == 1)
		if (mldNeedSingleBandMlsr56(ad, i))
			allow = TRUE;
#endif

		/* skip if not found matched link plan */
		if (!allow)
			continue;

		allow_plan_bitmap |= BIT(i);

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		link_plan = g_aeLinkPlan[i];

		/* 1,2,3 link */
		if (!apsMldBlockListAllow(ad, ap, i))
			allow_plan_bitmap &= ~BIT(i);
		for (j = 0; j < APS_LINK_MAX; j++) {
			int k;

			if (link_plan[j] == BAND_NULL)
				break;
			/* 1 link */
			curr_plan = apsSearchLinkPlan(ad,
					BIT(link_plan[j]),
					1);
			if (apsMldBlockListAllow(ad, ap, curr_plan))
				allow_plan_bitmap |= BIT(curr_plan);

			for (k = j + 1; k < APS_LINK_MAX; k++) {
				if (link_plan[k] == BAND_NULL)
					break;
				/* 2 link */
				curr_plan = apsSearchLinkPlan(ad,
					BIT(link_plan[j]) | BIT(link_plan[k]),
					2);
				if (apsMldBlockListAllow(ad, ap, curr_plan))
					allow_plan_bitmap |= BIT(curr_plan);
			}
		}
#endif
	}

	DBGLOG(APS, INFO, "CAND[%d]["MACSTR"] allow_plan = 0x%x\n",
			  ap->u4Index, MAC2STR(ap->aucAddr),
			  allow_plan_bitmap);

	/* select highest score link plan */
	for (i = 0; i < MLO_LINK_PLAN_NUM; i++) {
		struct BSS_DESC *candi[APS_LINK_MAX] = {0};
		uint8_t found = FALSE;
		uint32_t akm = 0;
		uint16_t best = 0;
		uint8_t link_num, rfband_bmap;

		/* skip disallowed plan */
		if (!(allow_plan_bitmap & BIT(i)))
			continue;

		/* reset picked flag */
		for (j = BAND_2G4; j < BAND_NUM; j++) {
			struct LINK *link = &ap->arLinks[j];

			LINK_FOR_EACH_ENTRY(bss, link,
				rLinkEntryEss[aidx], struct BSS_DESC)
				bss->fgPicked = FALSE;
		}

		link_plan = g_aeLinkPlan[i];
		for (j = 0; j < APS_LINK_MAX; j++) {
			if (link_plan[j] == BAND_NULL)
				continue;

			candi[j] = bss = apsIntraUpdateCandi(ad, ap,
			       link_plan[j], min_score, reason, FALSE, bidx);

			if (!bss)
				continue;

			found = TRUE;

			/* use akm of best ap as target akm */
			if (bss->u2Score > best) {
				best = bss->u2Score;
				akm = bss->u4RsnSelectedAKMSuite;
			}
		}

		/* skip if no candidate */
		if (!found)
			continue;

		/* use lower score to find links if already found one */
		for (j = 0; j < APS_LINK_MAX; j++) {
			if (link_plan[j] == BAND_NULL)
				continue;

			bss = candi[j];

			if (!bss)
				candi[j] = bss = apsIntraUpdateCandi(ad, ap,
				       link_plan[j], 0, reason, TRUE, bidx);

			if (bss && bss->u4RsnSelectedAKMSuite != akm) {
				DBGLOG(APS, INFO,
				       "Remove target akm 0x%x!=0x%x\n",
				       bss->u4RsnSelectedAKMSuite, akm);
				candi[j] = NULL;
			}
		}

		/* sort and trim candidates*/
		link_num = apsSortTrimCandiByScore(ad, candi,
			&curr_plan, &rfband_bmap);

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		if (!apsMldBlockListAllow(ad, ap, curr_plan))
			continue;
#endif

		if (reason == ROAMING_REASON_IDLE &&
		    rfband_bmap <= min_rfband_bmap)
			continue;

		apsUpdateTotalScore(ad,	candi, link_num, curr_plan, ap, bidx);

#if (CFG_SINGLE_BAND_MLSR_56 == 1)
		if (mldNeedSingleBandMlsr56(ad, curr_plan) && link_num == 2) {
			kalMemCopy(ap->aprTarget, candi, sizeof(ap->aprTarget));
			ap->ucLinkNum = 2;
			ap->eMloMode = MLO_MODE_SB_MLSR;
			ap->ucMaxSimuLinks = 0;
		}
#endif
	}
}

struct AP_COLLECTION *apsIntraApSelection(struct ADAPTER *ad,
	enum ENUM_ROAMING_REASON reason, uint8_t bidx)
{
	struct AIS_SPECIFIC_BSS_INFO *s = aisGetAisSpecBssInfo(ad, bidx);
	struct AIS_FSM_INFO *ais = aisGetAisFsmInfo(ad, bidx);
	struct LINK *ess = &s->rCurEssLink;
	struct AP_COLLECTION *ap, *nap, *current_ap = NULL;
	struct BSS_DESC *bss, *currBss = NULL;
	struct STA_RECORD *sta;
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct MLD_STA_RECORD *mldSta;
#endif
	uint16_t min_score = 0;
	uint8_t min_rfband_bmap = 0;
	int i, j, k;

	/* minimum requirement */
	for (i = 0; i < MLD_LINK_MAX; i++) {
		bss = aisGetLinkBssDesc(ais, i);
		sta = aisGetLinkStaRec(ais, i);
#if (CFG_SUPPORT_802_11BE_MLO == 1)
		mldSta = mldStarecGetByStarec(ad, sta);
#endif

		if (!bss || !sta)
			continue;

		bss->u2Score = apsCalculateApScore(ad, bss, reason, bidx);
		bss->u4Tput = apsGetEstimatedTput(ad, bss, bidx);
		min_rfband_bmap |= BIT(bss->eBand);

		DBGLOG(APS, INFO,
			"CURR[" MACSTR "] score[%d] tput[%d]\n",
			MAC2STR(bss->aucBSSID), bss->u2Score, bss->u4Tput);

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		if (mldSta == NULL) {
			currBss = bss;
			min_score = bss->u2Score;
		} else if ((mldSta->u4ActiveStaBitmap & BIT(sta->ucIndex)) &&
			   (currBss == NULL || bss->eBand > currBss->eBand)) {
			currBss = bss;
			min_score = bss->u2Score;
		}
#else
		currBss = bss;
		min_score = bss->u2Score;
#endif
	}

#if (CFG_SUPPORT_ROAMING_LOG == 1)
	roamingFsmLogCurr(ad, currBss, reason, bidx);
#endif

	if (currBss && !apsSanityCheckBssDesc(ad, currBss, reason, bidx))
		min_score = 0;

	LINK_FOR_EACH_ENTRY_SAFE(ap, nap,
			ess, rLinkEntry, struct AP_COLLECTION) {
		uint8_t rfband_bmap = 0;

		/* select best link plan */
		apsIntraSelectLinkPlan(ad, ap, min_score,
			min_rfband_bmap, reason, bidx);

		if (ap->ucLinkNum == 0)
			continue;

		for (i = 0, j = 0, k = 0; i < ap->ucLinkNum; i++) {
			struct BSS_DESC *cand = ap->aprTarget[i];
			uint8_t addr[MAC_ADDR_LEN] = {0};
			uint8_t *mld_addr = addr;
			uint32_t u4BlockBmap = 0;

			rfband_bmap |= BIT(cand->eBand);

			if (cand->prBlack)
				j++;

			if (IS_AIS_CONN_BSSDESC(ais, cand))
				k++;

#if (CFG_SUPPORT_802_11BE_MLO == 1)
			mld_addr = cand->rMlInfo.aucMldAddr;
			u4BlockBmap = ap->prBlock ?
				ap->prBlock->u4BlockBmap : 0;
#endif

			DBGLOG(APS, INFO,
				"CAND[%d] BSS[" MACSTR "] band[%s] mld[" MACSTR
				"] score[%d] tput[%d] conn[%d] bssid[%d] bssid_hint[%d] blk[%d] mld_blk[0x%x]\n",
				ap->u4Index,
				MAC2STR(cand->aucBSSID),
				apucBandStr[cand->eBand],
				MAC2STR(mld_addr),
				cand->u2Score, cand->u4Tput,
				cand->fgIsConnected,
				cand->fgIsMatchBssid,
				cand->fgIsMatchBssidHint,
				cand->prBlack != NULL,
				u4BlockBmap);
		}

		if (j == ap->ucLinkNum)
			ap->fgIsAllLinkInBlockList = TRUE;

		if (k == ap->ucLinkNum) {
			ap->fgIsAllLinkConnected = TRUE;
			current_ap = ap;
		}

		ap->eLinkPlan = apsSearchLinkPlan(ad,
			rfband_bmap, ap->ucLinkNum);

		DBGLOG(APS, INFO,
			"CAND[%d] num[%d,%s] score[%d] tput[%d] mode[%d] simu[%d] %s%s%s%s\n",
			ap->u4Index, ap->ucLinkNum,
			apuceLinkPlanStr[ap->eLinkPlan],
			ap->u4TotalScore, ap->u4TotalTput,
			ap->eMloMode, ap->ucMaxSimuLinks,
			ap->fgIsMatchBssid ? "(match_bssid)" : "",
			ap->fgIsMatchBssidHint ? "(match_bssid_hint)" : "",
			ap->fgIsAllLinkConnected ? "(connected)" : "",
			ap->fgIsAllLinkInBlockList ? "(in blocklist)" : "");
	}

	return current_ap;
}

uint32_t apsCalculateFinalScore(struct ADAPTER *ad,
	struct AP_COLLECTION *ap, enum ENUM_ROAMING_REASON reason,
	uint8_t bidx)
{
	uint32_t score = 0;

#if (CFG_EXT_ROAMING == 1)
	/* Customization */
	score = ap->u4TotalTput;
#else
	/* Common */
	switch (reason) {
#if CFG_SUPPORT_ROAMING
	case ROAMING_REASON_BTM: {
		uint8_t i;

		if (!aisCheckNeighborApValidity(ad, bidx)) {
			score = ap->u4TotalTput;
		} else {
			for (i = 0; i < ap->ucLinkNum; i++) {
				uint32_t pref;

				if (!ap->aprTarget[i])
					continue;

				pref = apsCalculateScoreByPreference(
					ad, ap->aprTarget[i], reason);

				/* select highest pref as tput score */
				if (score == 0 || pref > score)
					score = pref;
			}
		}
	}
		break;
#endif
	default:
		score = ap->u4TotalTput;
		break;
	}

#endif

	return score;
}

static uint8_t apsNeedReplaceCandidateByRssi(struct ADAPTER *prAdapter,
	struct BSS_DESC *prCandBss, struct BSS_DESC *prCurrBss,
	enum ENUM_ROAMING_REASON eRoamReason)
{
	int8_t cCandRssi = RCPI_TO_dBm(prCandBss->ucRCPI);
	int8_t cCurrRssi = RCPI_TO_dBm(prCurrBss->ucRCPI);
	enum ENUM_BAND eCurrBand = prCurrBss->eBand;
	enum ENUM_BAND eCandBand = prCandBss->eBand;

#if CFG_SUPPORT_NCHO
	if (prAdapter->rNchoInfo.fgNCHOEnabled)
		return cCurrRssi >= cCandRssi ? TRUE : FALSE;
#endif

	/* 1.3 Hard connecting RSSI check */
	if ((eCurrBand == BAND_5G && cCurrRssi < MINIMUM_RSSI_5G) ||
#if (CFG_SUPPORT_WIFI_6G == 1)
	   (eCurrBand == BAND_6G && cCurrRssi < MINIMUM_RSSI_6G) ||
#endif
	   (eCurrBand == BAND_2G4 && cCurrRssi < MINIMUM_RSSI_2G4))
		return FALSE;
	else if ((eCandBand == BAND_5G && cCandRssi < MINIMUM_RSSI_5G) ||
#if (CFG_SUPPORT_WIFI_6G == 1)
	   (eCandBand == BAND_6G && cCandRssi < MINIMUM_RSSI_6G) ||
#endif
	   (eCandBand == BAND_2G4 && cCandRssi < MINIMUM_RSSI_2G4))
		return TRUE;

	/* 1.4 prefer to select 5G Bss if Rssi of a 5G band BSS is good */
	if (eCandBand != eCurrBand) {
		switch (eCandBand) {
		case BAND_2G4:
			/* Current AP is 2.4G, replace candidate AP if target
			 * AP is good
			 */
			if (eCurrBand == BAND_5G
#if (CFG_SUPPORT_WIFI_6G == 1)
			 || eCurrBand == BAND_6G
#endif
			) {
				if (cCurrRssi >= GOOD_RSSI_FOR_HT_VHT)
					return TRUE;

				if (cCurrRssi < LOW_RSSI_FOR_5G_BAND &&
				   (cCandRssi > cCurrRssi + RSSI_DIFF_BIG_STEP))
					return FALSE;

				if (cCandRssi - cCurrRssi >= RSSI_DIFF_BIG_STEP)
					return FALSE;

				if (cCurrRssi - cCandRssi >= RSSI_DIFF_SML_STEP)
					return TRUE;
			}
			break;
		case BAND_5G:
			/* Candidate AP is 5G, don't replace it if it's
			 * good enough.
			 */
			if (eCurrBand == BAND_2G4) {
				if (cCandRssi >= GOOD_RSSI_FOR_HT_VHT)
					return FALSE;

				if (cCandRssi < LOW_RSSI_FOR_5G_BAND &&
				   (cCurrRssi > cCandRssi + RSSI_DIFF_BIG_STEP))
					return TRUE;

				if (cCandRssi - cCurrRssi >= RSSI_DIFF_SML_STEP)
					return FALSE;

				if (cCurrRssi - cCandRssi >= RSSI_DIFF_BIG_STEP)
					return TRUE;
			}
#if (CFG_SUPPORT_WIFI_6G == 1)
			else if (eCurrBand == BAND_6G) {
				/* Target AP is 6G, replace candidate AP
				 * if target AP is good
				 */
				if (cCurrRssi >= GOOD_RSSI_FOR_HT_VHT)
					return TRUE;

				if (cCurrRssi < LOW_RSSI_FOR_5G_BAND &&
				   (cCandRssi > cCurrRssi + RSSI_DIFF_MED_STEP))
					return FALSE;

				if (cCandRssi - cCurrRssi >= RSSI_DIFF_MED_STEP)
					return FALSE;

				if (cCurrRssi - cCandRssi >= RSSI_DIFF_SML_STEP)
					return TRUE;
			}
#endif
			break;
#if (CFG_SUPPORT_WIFI_6G == 1)
		case BAND_6G:
			/* Candidate AP is 6G, don't replace it if
			 * it's good enough.
			 */
			if (eCurrBand == BAND_2G4) {
				if (cCandRssi >= GOOD_RSSI_FOR_HT_VHT)
					return FALSE;

				if (cCandRssi < LOW_RSSI_FOR_5G_BAND &&
				   (cCurrRssi > cCandRssi + RSSI_DIFF_BIG_STEP))
					return TRUE;

				if (cCandRssi - cCurrRssi >= RSSI_DIFF_SML_STEP)
					return FALSE;

				if (cCurrRssi - cCandRssi >= RSSI_DIFF_BIG_STEP)
					return TRUE;
			} else if (eCurrBand == BAND_5G) {
				if (cCandRssi >= GOOD_RSSI_FOR_HT_VHT)
					return FALSE;

				if (cCandRssi < LOW_RSSI_FOR_5G_BAND &&
				   (cCurrRssi > cCandRssi + RSSI_DIFF_MED_STEP))
					return TRUE;

				if (cCandRssi - cCurrRssi >= RSSI_DIFF_SML_STEP)
					return FALSE;

				if (cCurrRssi - cCandRssi >= RSSI_DIFF_MED_STEP)
					return TRUE;
			}
			break;
#endif
		default:
			break;
		}
	} else {
		if (cCandRssi - cCurrRssi >= RSSI_DIFF_MED_STEP)
			return FALSE;
		if (cCurrRssi - cCandRssi >= RSSI_DIFF_MED_STEP)
			return TRUE;
	}

	return FALSE;
}

static uint8_t apsNeedReplaceByRssi(struct ADAPTER *ad,
	struct AP_COLLECTION *cand, struct AP_COLLECTION *curr,
	enum ENUM_ROAMING_REASON reason)
{
	uint8_t i, j;

	for (i = 0; i < curr->ucLinkNum; i++) {
		uint8_t better_rssi = TRUE;

		if (!curr->aprTarget[i])
			continue;

		for (j = 0; j < cand->ucLinkNum; j++) {
			if (!cand->aprTarget[j])
				continue;

			if (!apsNeedReplaceCandidateByRssi(ad,
				cand->aprTarget[j],
				curr->aprTarget[i],
				reason)) {
				better_rssi = FALSE;
				break;
			}
		}

		if (better_rssi)
			return TRUE;
	}

	return FALSE;
}

enum ENUM_APS_REPLACE_REASON apsInterNeedReplace(struct ADAPTER *ad,
	enum ENUM_PARAM_CONNECTION_POLICY policy,
	struct AP_COLLECTION *cand, struct AP_COLLECTION *curr,
	uint32_t cand_score, uint32_t curr_score,
	enum ENUM_ROAMING_REASON reason, uint8_t bidx)
{
	if (policy == CONNECT_BY_BSSID) {
		if (curr->fgIsMatchBssid)
			return APS_MATCH_BSSID;
		else
			return APS_UNMATCH_BSSID;
	} else if (policy == CONNECT_BY_BSSID_HINT) {
		if (curr->fgIsMatchBssidHint)
			return APS_MATCH_BSSID_HINT;
		if (cand && cand->fgIsMatchBssidHint)
			return APS_UNMATCH_BSSID_HINT;
	}

	if (!cand)
		return APS_FIRST_CANDIDATE;

#if (CFG_TC10_FEATURE == 0)
	if (reason == ROAMING_REASON_POOR_RCPI ||
	    reason == ROAMING_REASON_INACTIVE) {
		if (apsNeedReplaceByRssi(ad, cand, curr, reason))
			return APS_BETTER_RSSI;
		if (apsNeedReplaceByRssi(ad, curr, cand, reason))
			return APS_WORSE_RSSI;
	}
#endif

	return curr_score > cand_score ? APS_HIGH_SCORE : APS_LOW_SCORE;
}

struct BSS_DESC *apsFillBssDescSet(struct ADAPTER *ad,
	struct AP_COLLECTION *ap, struct BSS_DESC_SET *set, uint8_t bidx)
{
	struct CONNECTION_SETTINGS *conn = aisGetConnSettings(ad, bidx);
	struct AIS_FSM_INFO *ais;
	enum ENUM_PARAM_CONNECTION_POLICY policy = conn->eConnectionPolicy;
	uint8_t i;

	ais = aisGetAisFsmInfo(ad, bidx);

	if (!set)
		return ap ? ap->aprTarget[0] : NULL;

	kalMemSet(set, 0, sizeof(*set));
	if (!ap)
		goto done;

	for (i = 0; i < ap->ucLinkNum && set->ucLinkNum < MLD_LINK_MAX; i++) {
		if (!ap->aprTarget[i])
			continue;

		set->aprBssDesc[set->ucLinkNum++] = ap->aprTarget[i];
		set->ucRfBandBmap |= BIT(ap->aprTarget[i]->eBand);
	}

	if (policy == CONNECT_BY_BSSID)
		goto done;

#if (CFG_SUPPORT_802_11BE_MLO == 1)
	/* pick by special requirement
	 * and avoid picking the poor-quality link.
	 */
	for (i = 1; i < set->ucLinkNum; i++) {
		uint8_t *found = NULL;
		struct BSS_DESC *bss = set->aprBssDesc[i];

		if (IS_AIS_CONN_BSSDESC(ais, set->aprBssDesc[0]) &&
		    !(IS_AIS_CONN_BSSDESC(ais, bss))) {
			set->aprBssDesc[i] = set->aprBssDesc[0];
			set->aprBssDesc[0] = bss;
			found = "connected_link";
		} else if (set->aprBssDesc[0]->ucJoinFailureCount > 1 &&
		    !IS_AIS_CONN_BSSDESC(ais, bss) &&
		    bss->ucJoinFailureCount <
		    set->aprBssDesc[0]->ucJoinFailureCount) {
			set->aprBssDesc[i] = set->aprBssDesc[0];
			set->aprBssDesc[0] = bss;
			found = "bad_main_link";
		} else if (IS_FEATURE_ENABLED(ad->rWifiVar.ucStaPreferMldAddr)
			   && EQUAL_MAC_ADDR(bss->aucBSSID, ap->aucAddr)) {
			set->aprBssDesc[i] = set->aprBssDesc[0];
			set->aprBssDesc[0] = bss;
			found = "mld_addr";
		} else if (bss->rMlInfo.ucLinkIndex ==
			   ad->rWifiVar.ucStaMldMainLinkIdx) {
			set->aprBssDesc[i] = set->aprBssDesc[0];
			set->aprBssDesc[0] = bss;
			found = "link_id";
		}

		if (found != NULL) {
			DBGLOG(APS, INFO, MACSTR
				" link_id=%d max_links=%d Setup for %s\n",
				MAC2STR(bss->aucBSSID),
				bss->rMlInfo.ucLinkIndex,
				bss->rMlInfo.ucMaxSimuLinks,
				found);
			goto done;
		}
	}
#endif

done:
	/* first bss desc is main bss */
	set->prMainBssDesc = set->aprBssDesc[0];
	if (ap) {
		set->fgIsMatchBssid = ap->fgIsMatchBssid;
		set->fgIsMatchBssidHint = ap->fgIsMatchBssidHint;
		set->fgIsAllLinkInBlockList = ap->fgIsAllLinkInBlockList;
		set->fgIsAllLinkConnected = ap->fgIsAllLinkConnected;
		set->eMloMode = ap->eMloMode;
		set->ucMaxSimuLinks = ap->ucMaxSimuLinks;
	}
	DBGLOG(APS, INFO, "Total %d link(s)\n", set->ucLinkNum);
	return set->prMainBssDesc;
}

struct BSS_DESC *apsInterApSelection(struct ADAPTER *ad,
	struct BSS_DESC_SET *set,
	enum ENUM_ROAMING_REASON reason, uint8_t bidx,
	struct AP_COLLECTION *current_ap)
{
	struct CONNECTION_SETTINGS *conn = aisGetConnSettings(ad, bidx);
	enum ENUM_PARAM_CONNECTION_POLICY policy = conn->eConnectionPolicy;
	struct AIS_SPECIFIC_BSS_INFO *s = aisGetAisSpecBssInfo(ad, bidx);
	struct LINK *ess = &s->rCurEssLink;
	struct AP_COLLECTION *ap, *cand = NULL;
	uint32_t best = 0, score = 0;
	uint8_t try_blockList = FALSE;
	enum ENUM_APS_REPLACE_REASON replace_reason;

try_again:
	LINK_FOR_EACH_ENTRY(ap, ess, rLinkEntry, struct AP_COLLECTION) {
		if (ap->ucLinkNum == 0)
			continue;

		if (!try_blockList && ap->fgIsAllLinkInBlockList)
			continue;

		score = apsCalculateFinalScore(ad, ap, reason, bidx);
		replace_reason = apsInterNeedReplace(ad, policy,
			cand, ap, best, score, reason, bidx);
		if (replace_reason >= APS_NEED_REPLACE) {
			best = score;
			cand = ap;
		}

		DBGLOG(APS, INFO,
			"%s CAND[%d] %s[" MACSTR
			"] num[%d,%s] score[%d] (%s)\n",
			replace_reason >= APS_FIRST_CANDIDATE ? "--->" : "<---",
			ap->u4Index, ap->fgIsMld ? "MLD" : "BSS",
			MAC2STR(ap->aucAddr), ap->ucLinkNum,
			apuceLinkPlanStr[ap->eLinkPlan], score,
			replace_reason < APS_REPLACE_REASON_NUM ?
			apucReplaceReasonStr[replace_reason] :
			(const uint8_t *)"UNKNOWN");

		/* early leave for specific reasons */
		if (replace_reason == APS_MATCH_BSSID ||
		    replace_reason == APS_MATCH_BSSID_HINT)
			goto done;
	}

	if (!try_blockList && (!cand || cand->fgIsAllLinkConnected)) {
		try_blockList = TRUE;
		DBGLOG(APS, INFO, "No ap collection found, try blocklist\n");
		goto try_again;
	}

done:
	return apsFillBssDescSet(ad, cand, set, bidx);
}

struct BSS_DESC *apsSearchBssDescByScore(struct ADAPTER *ad,
	enum ENUM_ROAMING_REASON reason,
	uint8_t bidx, struct BSS_DESC_SET *set)
{
	struct AIS_SPECIFIC_BSS_INFO *s = aisGetAisSpecBssInfo(ad, bidx);
	struct LINK *ess = &s->rCurEssLink;
	struct CONNECTION_SETTINGS *conn = aisGetConnSettings(ad, bidx);
	struct BSS_DESC *cand = NULL;
	struct AP_COLLECTION *current_ap = NULL;
	uint16_t count = 0;

	if (reason >= ROAMING_REASON_NUM) {
		DBGLOG(APS, ERROR, "reason %d!\n", reason);
		return NULL;
	}

	DBGLOG(APS, INFO, "ConnectionPolicy = %d, reason = %d\n",
		conn->eConnectionPolicy, reason);

	aisRemoveTimeoutBlocklist(ad, AIS_BLOCKLIST_TIMEOUT);
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	aisRemoveTimeoutMldBlocklist(ad, AIS_BLOCKLIST_TIMEOUT);
#endif

#if CFG_SUPPORT_802_11K
	/* check before using neighbor report */
	aisCheckNeighborApValidity(ad, bidx);
#endif

	count = apsUpdateEssApList(ad, reason, bidx);
	current_ap = apsIntraApSelection(ad, reason, bidx);
	cand = apsInterApSelection(ad, set, reason, bidx, current_ap);
	if (cand) {
		if (cand->eBand < 0 || cand->eBand >= BAND_NUM) {
			DBGLOG(APS, WARN, "Invalid Band %d\n", cand->eBand);
		} else {
			DBGLOG(APS, INFO,
				"Selected "
				MACSTR ", RSSI[%d] Band[%s] when find %s, "
				MACSTR " policy=%d in %d(%d) BSSes.\n",
				MAC2STR(cand->aucBSSID),
				RCPI_TO_dBm(cand->ucRCPI),
				apucBandStr[cand->eBand], HIDE(conn->aucSSID),
				conn->eConnectionPolicy == CONNECT_BY_BSSID ?
				MAC2STR(conn->aucBSSID) :
				MAC2STR(conn->aucBSSIDHint),
				conn->eConnectionPolicy,
				count,
				ess->u4NumElem);
			goto done;
		}
	}

	DBGLOG(APS, INFO, "Selected None when find %s, " MACSTR
		" in %d(%d) BSSes.\n",
		conn->aucSSID, MAC2STR(conn->aucBSSID),
		count,
		ess->u4NumElem);
done:
#if (CFG_SUPPORT_ROAMING_LOG == 1)
	roamingFsmLogResult(ad, bidx, cand);
#endif
	apsResetEssApList(ad, bidx);
	return cand;
}

