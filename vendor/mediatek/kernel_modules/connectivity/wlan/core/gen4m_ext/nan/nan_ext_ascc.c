/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 ** Id: /nan/nan_ext_ascc.c
 */

/*! \file   "nan_ext_ascc.c"
 *  \brief  This file defines the procedures handling AScC commands.
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
#include "cnm_mem.h"
#include <linux/rtc.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/net.h>


#include "nan_base.h"
#include "nanScheduler.h"
#include "nan_ext_ascc.h"

#if CFG_SUPPORT_NAN_EXT

/* Parse command and set interval in FW, just leave code, just keep the code */
#define CHECK_DW 0

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define INVALID_SLOT_INDEX 0xFF /* arNdpEntry */

enum _ENUM_BAND_INDEX_T {
	NAN_EXT_BAND_2G4, /* 0 */
	NAN_EXT_BAND_5G, /* 1 */
	NAN_EXT_BAND_6G, /* 2 */
	NAN_EXT_NUM_2_BANDS = NAN_EXT_BAND_6G, /* 2 */
	NAN_EXT_NUM_3_BANDS, /* 3 */
	NAN_EXT_BAND_NOT_DEFINED, /* 4 */
	NAN_EXT_BAND_INVALID, /* 5 */
};

/**
 * (NAN_2G_IDX == NAN_EXT_BAND_2G4 && NAN_5G_IDX == NAN_EXT_BAND_5G &&
 *  NAN_6G_IDX == NAN_EXT_BAND_6G),
 * "Check _ENUM_BAND_INDEX_T".
 */

/* convert from _ENUM_BAND_INDEX_T(0-index) to ENUM_BAND (1-index) */
static const uint8_t bandTable[] = {
	[NAN_EXT_BAND_2G4] = BAND_2G4,
	[NAN_EXT_BAND_5G] = BAND_5G,
	[NAN_EXT_BAND_6G] = BAND_6G,
};

/* DW */
#define DW_ENTRY_BITMAP  \
	(BIT(CATEGORY_DW_2G) | BIT(CATEGORY_DW_5G) | BIT(CATEGORY_DW_6G))

/* Social: FC (FAW), USD, SD */
#define SOCIAL_ENTRY_BITMAP  \
	(BIT(CATEGORY_FC_2G) | BIT(CATEGORY_FC_5G) | BIT(CATEGORY_FC_6G) | \
	 BIT(CATEGORY_USD_2G) | BIT(CATEGORY_USD_5G) | \
	 BIT(CATEGORY_SD_2G) | BIT(CATEGORY_SD_5G))

/* AP (FAW) */
#define AP_ENTRY_BITMAP  \
	(BIT(CATEGORY_AP_2G) | BIT(CATEGORY_AP_5G) | BIT(CATEGORY_AP_6G))

/* NDP (FAW) */
#define NDP_ENTRY_BITMAP  \
	(BIT(CATEGORY_NDP_FAW_2G) | BIT(CATEGORY_NDP_FAW_5G) | \
	 BIT(CATEGORY_NDP_FAW_6G))

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/**
 * Save the whole structure to fill the check schedule response
 * struct sched_entry_dw {
 *	uint8_t channel; * 2.4G: 1~14; 5G: 1~196, 6G: 1~233 *
 *	uint16_t offset: 5; * unit: 16 TU, up to 31 (i.e., 496 TU) *
 *	uint16_t wakeup_interval: 4; * 256 << n *
 *	uint16_t beacon: 1; * whether to TX discovery beacon *
 *};
 *
 * struct sched_entry_faw {
 *	 * Including specified band, 4 types, 2/5/6Gx{FC} + FW scan
 *	 * NDP settings are merged to common part;
 *	 * AP settings are controlled by setting timeline to CNM
 *	 *
 *	uint8_t channel;
 *	uint32_t bitmap;
 *};
 *
 * Same structure by both SD and USD with 2 bands respectively
 * struct sched_entry_sd_usd {
 *	uint8_t channel;
 *	uint8_t timeout;
 *	uint8_t unsol_sdf_num: 4,
 *		interval: 4;
 *	uint32_t bitmap;
 *};
 *
 * struct sched_entry_ulw {
 *	* including specified band, 4 types, 2/5/6G ULW + passive scan *
 *	uint32_t start_time;
 *	uint32_t duration;
 *	uint32_t period;
 *	uint32_t countdown;
 *	uint8_t channel;
 *	uint8_t nan_present: 1;
 * };
 *
 * struct sched_entry_discovery {
 *	* including specified band, 4 types, 2/5/6G Discovery *
 *	uint8_t mscp_interval;
 * };
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

static uint32_t nanDwEntryHandler(struct ADAPTER *prAdapter,
				  void *cmd,
				  const uint8_t *buf,
				  u_int8_t update);

static uint32_t nanFawEntryHandler(struct ADAPTER *prAdapter,
				   void *cmd,
				   const uint8_t *buf,
				   u_int8_t update);

static uint32_t nanUsdEntryHandler(struct ADAPTER *prAdapter,
				   void *cmd,
				   const uint8_t *buf,
				   u_int8_t update);

static uint32_t nanSdEntryHandler(struct ADAPTER *prAdapter,
				  void *cmd,
				  const uint8_t *buf,
				  u_int8_t update);

static uint32_t nanUlwEntryHandler(struct ADAPTER *prAdapter,
				   void *cmd,
				   const uint8_t *buf,
				   u_int8_t update);

static uint32_t nanDiscoveryEntryHandler(struct ADAPTER *prAdapter,
					 void *cmd,
					 const uint8_t *buf,
					 u_int8_t update);

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

static struct LINK g_rPendingReqList;

static struct custom_schedule_entry {
	uint8_t usage: 4, /* */
		data_tx_start_time: 2, /* 0: 0DW; 1: 1DW; 2: 2DW */
		ndp_idle_timer_paused: 1; /* 0: enable NDP idle timer */

	/* uint8_t src_mac[6]; TODO: currently only one local address */
	uint8_t dst_mac[6]; /**/
	uint32_t custom_schedule_entry;

	struct IE_NAN_ASCC_DW arDwEntry[NAN_EXT_NUM_3_BANDS];
	struct IE_NAN_ASCC_FAW arFcEntry[NAN_EXT_NUM_3_BANDS];
	/* Additional one for storing command given cluster ID */
	uint32_t u4NdpEntryConnSetBitmap;
	struct IE_NAN_ASCC_FAW arNdpEntry
				[NAN_EXT_NUM_3_BANDS][NAN_MAX_CONN_CFG + 1];
	struct IE_NAN_ASCC_FAW arApEntry[NAN_EXT_NUM_3_BANDS];
	struct IE_NAN_ASCC_FAW rFwScanEntry;
	struct IE_NAN_ASCC_USD arUsdEntry[NAN_EXT_NUM_2_BANDS];
	struct IE_NAN_ASCC_SD arSdEntry[NAN_EXT_NUM_2_BANDS];
	struct IE_NAN_ASCC_ULW arUlwEntry[NAN_EXT_NUM_3_BANDS];
	struct IE_NAN_ASCC_ULW rPassiveScanEntry;
	struct IE_NAN_ASCC_DISCOVERY arDiscoveryEntry[NAN_EXT_NUM_3_BANDS];
} gAsccRecord;

union schedule_entries {
	struct IE_NAN_ASCC_DW dw;	/* 4 bytes */
	struct IE_NAN_ASCC_FAW faw;	/* 6 bytes */
	struct IE_NAN_ASCC_USD usd;	/* 8 bytes */
	struct IE_NAN_ASCC_SD sd;	/* 8 bytes */
	struct IE_NAN_ASCC_ULW ulw;	/* 20 bytes */
	struct IE_NAN_ASCC_DISCOVERY discovery; /* 12 bytes */
};

struct NAN_SCHEDULE_ENTRY {
	const char *str; /* getScheduleEntryString() */
	enum _ENUM_BAND_INDEX_T band; /* getScheduleEntryBand() */
	size_t size; /* getScheduleEntrySize() */
	union schedule_entries default_sched_entry;
	uint32_t (* const handler)(struct ADAPTER *,
				   void *, /* cmd */
				   const uint8_t *,
				   u_int8_t);
	void *prScheduleEntry; /* getScheduleEntryPtr() */
};

static const struct NAN_SCHEDULE_ENTRY nanExtSchedEntry[] = {
	/* Table 6. Schedule entry for DW */
	[CATEGORY_DW_2G] = { /* 0 */
		.str = "2.4G DW",
		.band = NAN_EXT_BAND_2G4,
		.size = sizeof(struct IE_NAN_ASCC_DW),
		.default_sched_entry = {
			.dw.schedule_category = CATEGORY_DW_2G,
			.dw.operation = NAN_SCHEDULE_ENTRY_ADD,
			.dw.start_offset = NAN_2G_DW_INDEX,
			.dw.ucOpChannel = NAN_2P4G_DISC_CHANNEL, /* 6 */
			.dw.dw_wakeup_interval = 1, /* 512 TUs */
			.dw.discovery_beacon_enabled = 1,
		},
		.handler = nanDwEntryHandler,
		.prScheduleEntry = &gAsccRecord.arDwEntry[NAN_EXT_BAND_2G4],
	},
	[CATEGORY_DW_5G] = { /* 3 */
		.str = "5G DW",
		.band = NAN_EXT_BAND_5G,
		.size = sizeof(struct IE_NAN_ASCC_DW),
		.default_sched_entry = {
			.dw.schedule_category = CATEGORY_DW_5G,
			.dw.operation = NAN_SCHEDULE_ENTRY_ADD,
			.dw.start_offset = NAN_5G_DW_INDEX,
			.dw.ucOpChannel = NAN_5G_HIGH_DISC_CHANNEL, /* 149 */
			.dw.dw_wakeup_interval = 1, /* 512 TUs */
			.dw.discovery_beacon_enabled = 1,
		},
		.handler = nanDwEntryHandler,
		.prScheduleEntry = &gAsccRecord.arDwEntry[NAN_EXT_BAND_5G],
	},
	[CATEGORY_DW_6G] = { /* 6 */
		.str = "6G DW",
		.band = NAN_EXT_BAND_6G,
		.size = sizeof(struct IE_NAN_ASCC_DW),
		.default_sched_entry = {
			.dw.schedule_category = CATEGORY_DW_6G,
			.dw.operation = NAN_SCHEDULE_ENTRY_ADD,
			/* TODO:
			 * .dw.start_offset =
			 * .dw.ucOpChannel =
			 * .dw.dw_wakeup_interval = 1,
			 * .dw.discovery_beacon_enabled = 1,
			 */
		},
		.handler = nanDwEntryHandler,
		.prScheduleEntry = &gAsccRecord.arDwEntry[NAN_EXT_BAND_6G],
	},

	/* Table 7. Schedule entry for FAW */
	[CATEGORY_FC_2G] = { /* 1 */
		.str = "2.4G FC",
		.band = NAN_EXT_BAND_2G4,
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {
			.faw.schedule_category = CATEGORY_FC_2G,
			.faw.operation = NAN_SCHEDULE_ENTRY_ADD,
			.faw.u4Bitmap = 0x0,
			.faw.ucOpChannel = NAN_2P4G_DISC_CHANNEL, /* 6 */
		},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.arFcEntry[NAN_EXT_BAND_2G4],
		/* struct IE_NAN_ASCC_FAW */
	},
	[CATEGORY_FC_5G] = { /* 4 */
		.str = "5G FC",
		.band = NAN_EXT_BAND_5G,
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {
			.faw.schedule_category = CATEGORY_FC_5G,
			.faw.operation = NAN_SCHEDULE_ENTRY_ADD,
			.faw.u4Bitmap = 0x0,
			.faw.ucOpChannel = NAN_5G_HIGH_DISC_CHANNEL, /* 149 */
		},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.arFcEntry[NAN_EXT_BAND_5G],
	},
	[CATEGORY_FC_6G] = { /* 7 */
		.str = "6G FC",
		.band = NAN_EXT_BAND_6G,
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {
			.faw.schedule_category = CATEGORY_FC_6G,
			.faw.operation = NAN_SCHEDULE_ENTRY_ADD,
			.faw.u4Bitmap = 0x0,
			/* TODO:
			 * .faw.ucOpChannel = NAN_5G_HIGH_DISC_CHANNEL, ?
			 */
		},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.arFcEntry[NAN_EXT_BAND_6G],
	},

	[CATEGORY_NDP_FAW_2G] = { /* 2 */
		.str = "2.4G NDP (FAW)",
		.band = NAN_EXT_BAND_2G4,
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {
			/* No need to pass command to FW */
			/* Reset per-peer NDL settings in driver */
		},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.arNdpEntry[NAN_EXT_BAND_2G4],
		/* struct IE_NAN_ASCC_FAW */
	},
	[CATEGORY_NDP_FAW_5G] = { /* 5 */
		.str = "5G NDP (FAW)",
		.band = NAN_EXT_BAND_5G,
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {
			/* No need to pass command to FW */
			/* Reset per-peer NDL settings in driver */
		},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.arNdpEntry[NAN_EXT_BAND_5G],
	},
	[CATEGORY_NDP_FAW_6G] = { /* 8 */
		.str = "6G NDP (FAW)",
		.band = NAN_EXT_BAND_6G,
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {
			/* No need to pass command to FW */
			/* Reset per-peer NDL settings in driver */
		},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.arNdpEntry[NAN_EXT_BAND_6G],
	},

	[CATEGORY_AP_2G] = { /* 9 */
		.str = "2.4G AP",
		.band = NAN_EXT_BAND_2G4,
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {
			/* No need to pass command to FW */
			/* Reset AIS bitmap settings in driver */
		},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.arApEntry[NAN_EXT_BAND_2G4],
	},
	[CATEGORY_AP_5G] = { /* 10 */
		.str = "5G AP",
		.band = NAN_EXT_BAND_5G,
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {
			/* No need to pass command to FW */
			/* Reset AIS bitmap settings in driver */
		},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.arApEntry[NAN_EXT_BAND_5G],
	},
	[CATEGORY_AP_6G] = { /* 11 */
		.str = "6G AP",
		.band = NAN_EXT_BAND_6G,
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {
			/* No need to pass command to FW */
			/* Reset AIS bitmap settings in driver */
		},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.arApEntry[NAN_EXT_BAND_6G],
	},

	[CATEGORY_FW_TRIGGERED_SCAN] = { /* 17 */
		.str = "Firmware triggered scan",
		.band = NAN_EXT_BAND_NOT_DEFINED, /* Firmware triggered scan */
		.size = sizeof(struct IE_NAN_ASCC_FAW),
		.default_sched_entry = {/* TODO */},
		.handler = nanFawEntryHandler,
		.prScheduleEntry = &gAsccRecord.rFwScanEntry,
	},

	/* Table 8. Schedule entry for USD */
	[CATEGORY_USD_2G] = { /* 12 */
		.str = "2.4G USD",
		.band = NAN_EXT_BAND_2G4,
		.size = sizeof(struct IE_NAN_ASCC_USD),
		.default_sched_entry = {
			/* Reset timeline in driver */
			/* Pass command to FW to update AScC context */
			/* TODO: apply proper default USD setting */
			.usd.schedule_category = CATEGORY_USD_2G,
			.usd.operation = NAN_SCHEDULE_ENTRY_ADD,
			.usd.u4Bitmap = 0x00000000,
			.usd.ucOpChannel = NAN_2P4G_DISC_CHANNEL,
			.usd.ucUsdTimeout = 0,
			.usd.number_unsoliciated_sdf = 0,
			.usd.usd_interval = 0,
		},
		.handler = nanUsdEntryHandler,
		.prScheduleEntry = &gAsccRecord.arUsdEntry[NAN_EXT_BAND_2G4],
	},
	[CATEGORY_USD_5G] = { /* 13 */
		.str = "5G USD",
		.band = NAN_EXT_BAND_5G,
		.size = sizeof(struct IE_NAN_ASCC_USD),
		.default_sched_entry = {
			/* Reset timeline in driver */
			/* Pass command to FW to update AScC context */
			/* TODO: apply proper default USD setting */
			.usd.schedule_category = CATEGORY_USD_2G,
			.usd.operation = NAN_SCHEDULE_ENTRY_ADD,
			.usd.u4Bitmap = 0x00000000,
			.usd.ucOpChannel = NAN_5G_HIGH_DISC_CHANNEL,
			.usd.ucUsdTimeout = 0,
			.usd.number_unsoliciated_sdf = 0,
			.usd.usd_interval = 0,
		},
		.handler = nanUsdEntryHandler,
		.prScheduleEntry = &gAsccRecord.arUsdEntry[NAN_EXT_BAND_5G],
	},

	/* Table 9. Schedule entry for SD */
	[CATEGORY_SD_2G] = { /* 14 */
		.str = "2.4G SD",
		.band = NAN_EXT_BAND_2G4,
		.size = sizeof(struct IE_NAN_ASCC_SD),
		.default_sched_entry = {
			/* Reset timeline in driver */
			/* Pass command to FW to update AScC context */
			/* TODO: apply proper default SD setting */
			.sd.schedule_category = CATEGORY_SD_2G,
			.sd.operation = NAN_SCHEDULE_ENTRY_ADD,
			.sd.u4Bitmap = 0x00000000,
			.sd.ucOpChannel = NAN_2P4G_DISC_CHANNEL,
			.sd.number_unsoliciated_sdf = 0,
			.sd.sd_interval = 0,
			.sd.ucSdTimeout = 0,
		},
		.handler = nanSdEntryHandler,
		.prScheduleEntry = &gAsccRecord.arSdEntry[NAN_EXT_BAND_2G4],
	},
	[CATEGORY_SD_5G] = { /* 15 */
		.str = "5G SD",
		.band = NAN_EXT_BAND_5G,
		.size = sizeof(struct IE_NAN_ASCC_SD),
		.default_sched_entry = {
			/* Reset timeline in driver */
			/* Pass command to FW to update AScC context */
			.sd.schedule_category = CATEGORY_SD_5G,
			.sd.operation = NAN_SCHEDULE_ENTRY_ADD,
			.sd.u4Bitmap = 0x00000000,
			.sd.ucOpChannel = NAN_5G_HIGH_DISC_CHANNEL,
			.sd.number_unsoliciated_sdf = 0,
			.sd.sd_interval = 0,
			.sd.ucSdTimeout = 0,
		},
		.handler = nanSdEntryHandler,
		.prScheduleEntry = &gAsccRecord.arSdEntry[NAN_EXT_BAND_5G],
	},

	/* Table 10. Schedule entry for ULW */
	[CATEGORY_PASSIVE_SCAN] = { /* 16 */
		.str = "Passive scan",
		.band = NAN_EXT_BAND_NOT_DEFINED, /* TODO: Passive scan */
		.size = sizeof(struct IE_NAN_ASCC_ULW),
		.default_sched_entry = {/* TODO */},
		.handler = nanUlwEntryHandler,
		.prScheduleEntry = &gAsccRecord.rPassiveScanEntry,
	},
	[CATEGORY_ULW_2G] = { /* 18 */
		.str = "2.4G ULW",
		.band = NAN_EXT_BAND_2G4,
		.size = sizeof(struct IE_NAN_ASCC_ULW),
		.default_sched_entry = {/* TODO */},
		.handler = nanUlwEntryHandler,
		.prScheduleEntry = &gAsccRecord.arUlwEntry[NAN_EXT_BAND_2G4],
	},
	[CATEGORY_ULW_5G] = { /* 19 */
		.str = "5G ULW",
		.band = NAN_EXT_BAND_5G,
		.size = sizeof(struct IE_NAN_ASCC_ULW),
		.default_sched_entry = {/* TODO */},
		.handler = nanUlwEntryHandler,
		.prScheduleEntry = &gAsccRecord.arUlwEntry[NAN_EXT_BAND_5G],
	},
	[CATEGORY_ULW_6G] = { /* 20 */
		.str = "6G ULW",
		.band = NAN_EXT_BAND_6G,
		.size = sizeof(struct IE_NAN_ASCC_ULW),
		.default_sched_entry = {/* TODO */},
		.handler = nanUlwEntryHandler,
		.prScheduleEntry = &gAsccRecord.arUlwEntry[NAN_EXT_BAND_6G],
	},

	/* v2.0 Table 11. Schedule entry for Discovery */
	[CATEGORY_DISCOVERY_2G] = { /* 21 */
		.str = "2.4G Common Discovery Schedule",
		.band = NAN_EXT_BAND_2G4,
		.size = sizeof(struct IE_NAN_ASCC_DISCOVERY),
		.default_sched_entry = {/* TODO */},
		.handler = nanDiscoveryEntryHandler,
		.prScheduleEntry =
			&gAsccRecord.arDiscoveryEntry[NAN_EXT_BAND_2G4],
	},
	[CATEGORY_DISCOVERY_5G] = { /* 22 */
		.str = "5G Common Discovery Schedule",
		.band = NAN_EXT_BAND_5G,
		.size = sizeof(struct IE_NAN_ASCC_DISCOVERY),
		.default_sched_entry = {/* TODO */},
		.handler = nanDiscoveryEntryHandler,
		.prScheduleEntry =
			&gAsccRecord.arDiscoveryEntry[NAN_EXT_BAND_5G],
	},
	[CATEGORY_DISCOVERY_6G] = { /* 23 */
		.str = "6G Common Discovery Schedule",
		.band = NAN_EXT_BAND_6G,
		.size = sizeof(struct IE_NAN_ASCC_DISCOVERY),
		.default_sched_entry = {/* TODO */},
		.handler = nanDiscoveryEntryHandler,
		.prScheduleEntry =
			&gAsccRecord.arDiscoveryEntry[NAN_EXT_BAND_6G],
	},
};

_Static_assert(ARRAY_SIZE(nanExtSchedEntry) <=
	       sizeof(gAsccRecord.custom_schedule_entry) * CHAR_BIT,
	       "Too many Schedule Entries");

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

static const char *getScheduleEntryString(uint8_t category)
{
	if  (category < ARRAY_SIZE(nanExtSchedEntry))
		return nanExtSchedEntry[category].str;

	return "Invalid";
}

/* Note:  ENUM_BAND (BAND_2G4, BAND_5G, BAND_6G) */
/**
 * getScheduleEntryBand() - Get associated band of the schedule category
 * @category: of the schedule entry
 *
 * Return the band of the schedule entry (2.4, 5, 6 GHz) in 0-index
 * enum _ENUM_BAND_INDEX_T to be used for acceing array index in storage.
 *
 * Context:
 * NOTE, need to convert it to enum ENUM_BAND for some utility functions which
 * might use 1-index band argument.
 *
 * Return: the corresponding band associated with the schedule category,
 * or NAN_EXT_BAND_INVALID if out of range.
 */
enum _ENUM_BAND_INDEX_T getScheduleEntryBand(uint8_t category)
{
	if  (category < ARRAY_SIZE(nanExtSchedEntry))
		return nanExtSchedEntry[category].band;

	return NAN_EXT_BAND_INVALID;
}

static size_t getScheduleEntrySize(uint32_t schedule_entry_id)
{
	if (schedule_entry_id < ARRAY_SIZE(nanExtSchedEntry))
		return nanExtSchedEntry[schedule_entry_id].size;

	return 0;
}

static void *getScheduleEntryPtr(uint32_t schedule_entry_id)
{
	if (schedule_entry_id < ARRAY_SIZE(nanExtSchedEntry))
		return nanExtSchedEntry[schedule_entry_id].prScheduleEntry;

	return NULL;
}

const char *schedule_category_included_str(uint32_t c, char *buf, uint32_t size)
{
	uint8_t i;
	uint32_t n = 0;
	char *end = buf + size;

	for (i = 0; i < 32; i++) {
		if (c & BIT(i))
			n += snprintf(buf + n, end - buf,
			"%u %s, ", i, getScheduleEntryString(i));
	}
	return buf;
}

const char *operation_str(uint8_t i)
{
	static const char * const operation_string[] = {
		[0] = "Add schedule",
		[1] = "Remove schedule",
		[2] = "Overwrite schedule",
		[3] = "Reset schedule",
		[4] = "Not changed",
		[5] = "Reserved",
		[6] = "Reserved",
		[7] = "Failed to change schedule",
	};

	if  (i < ARRAY_SIZE(operation_string))
		return operation_string[i];

	return "Invalid";
}

/**
 * Table 6. DW wakeup interval field
 * 0: 256 TU
 * 1: 512 TU
 * 2: 1024 TU
 * 3: 2048 TU
 * 4: 4096 TU
 * 5: 8192 TU
 * 6~14: reserved
 * 15: Do not wake up at DW
 */
static uint32_t calculate_dw_wakeup_interval(uint8_t dw_wakeup_interval)
{
	if (dw_wakeup_interval < 6) /* 512 * 2 ^ (n-1) */
		return (256U << dw_wakeup_interval);
	else if (dw_wakeup_interval == 0xF) /* Do not wake up at DW */
		return UINT_MAX;
	else /* Reserved */
		return 0;
}

#if (CHECK_DW == 1)
/* Parse command and set interval in FW, just leave code, just keep the code */
/**
 * is_slot_at_DW() - check whether given slot is a DW
 * @offset: range(0, 32) in the unit of 16TU
 * @dw_interval: [256, 512, 1024, 2048, 4096, 8192],
 * @slot: range(0, 512), in the unit of 16TU, representing up to 8192 TUs
 *
 * Return: TRUE if slot == offset + (dw_interval * k); else FALSE.
 */
uint32_t is_slot_at_DW(uint32_t offset, uint32_t dw_interval, uint32_t slot)
{
	uint32_t interval;

	interval = dw_interval / NAN_SLOT_INTERVAL; /* align all to 16 TUs */

	return slot >= offset && (slot - offset) % interval  == 0;
}

/**
 * is_slot_at_FAW() - check whether given slot is a FAW enabled slot
 * @bitmap: each bit means presence of timing of a NAN time slot
 * @slot: range(0, 512), in the unit of 16TU, representing up to 8192 TUs
 *
 * Return: TRUE if slot == offset + (dw_interval * k); else FALSE.
 */
uint32_t is_slot_at_FAW(uint32_t bitmap, uint32_t slot)
{
	return bitmap & (slot % NAN_SLOTS_PER_DW_INTERVAL);
}
#endif

/* schedule_category_bitmap: bitmap of schedule entries */
static u_int8_t isNanDwScheduleEntrySet(uint32_t schedule_category_bitmap)
{
	return !!(schedule_category_bitmap & DW_ENTRY_BITMAP);
}

/* schedule_category_bitmap: bitmap of schedule entries */
static u_int8_t isNanSocialScheduleEntrySet(uint32_t schedule_category_bitmap)
{

	return !!(schedule_category_bitmap & SOCIAL_ENTRY_BITMAP);
}

/* schedule_category_bitmap: bitmap of schedule entries */
static u_int8_t isAisScheduleEntrySet(uint32_t schedule_category_bitmap)
{

	return !!(schedule_category_bitmap & AP_ENTRY_BITMAP);
}

/* schedule_category: bitmap of schedule entries */
static u_int8_t isNdpScheduleEntrySet(uint32_t schedule_category_bitmap)
{

	return !!(schedule_category_bitmap & NDP_ENTRY_BITMAP);
}

/* schedule_category: value (within enum NAN_SCHEDULE_CATEGORY) */
static u_int8_t isNanDwScheduleEntry(uint32_t schedule_category)
{
	if (unlikely(schedule_category >=
		     sizeof(gAsccRecord.custom_schedule_entry) * CHAR_BIT))
		return FALSE;

	return isNanDwScheduleEntrySet(BIT(schedule_category));
}

/* FC, USD, SD */
/* schedule_category: value (within enum NAN_SCHEDULE_CATEGORY) */
static u_int8_t isNanSocialScheduleEntry(uint32_t schedule_category)
{
	if (unlikely(schedule_category >=
		     sizeof(gAsccRecord.custom_schedule_entry) * CHAR_BIT))
		return FALSE;

	return isNanSocialScheduleEntrySet(BIT(schedule_category));
}

/* schedule_category: value (within enum NAN_SCHEDULE_CATEGORY) */
static u_int8_t isAisScheduleEntry(uint32_t schedule_category)
{
	if (unlikely(schedule_category >=
		     sizeof(gAsccRecord.custom_schedule_entry) * CHAR_BIT))
		return FALSE;

	return isAisScheduleEntrySet(BIT(schedule_category));
}

/* schedule_category: value (within enum NAN_SCHEDULE_CATEGORY) */
static u_int8_t isNdpScheduleEntry(uint32_t schedule_category)
{
	if (unlikely(schedule_category >=
		     sizeof(gAsccRecord.custom_schedule_entry) * CHAR_BIT))
		return FALSE;

	return isNdpScheduleEntrySet(BIT(schedule_category));
}

static void nanUpdateDw(struct IE_NAN_ASCC_DW *dw)
{
	struct IE_NAN_ASCC_DW *pDw;
	enum _ENUM_BAND_INDEX_T band;

	band = getScheduleEntryBand(dw->schedule_category);
	if (band >= ARRAY_SIZE(gAsccRecord.arDwEntry))
		return;

	gAsccRecord.custom_schedule_entry |= BIT(dw->schedule_category);
	DBGLOG(NAN, INFO, "set %u, custom_schedule_entry=0x%08x\n",
	       dw->schedule_category,
	       gAsccRecord.custom_schedule_entry);

	pDw = &gAsccRecord.arDwEntry[band];
	/* TODO: write default if not set */
	kalMemCopy(pDw, dw, sizeof(*dw));

	if (pDw->dw_wakeup_interval == 0)
		DBGLOG(NAN, WARN, "unsupported DW interval %u",
		       pDw->dw_wakeup_interval);
	if (pDw->dw_wakeup_interval == 0xF)
		DBGLOG(NAN, WARN, "DW interval %u supported?",
		       pDw->dw_wakeup_interval);

	DBGLOG(NAN, INFO,
	       "Update DW[%u] ch=%u, offset=%u, interval=%u, beacon=%u\n",
	       band, pDw->ucOpChannel, pDw->start_offset,
	       pDw->dw_wakeup_interval, pDw->discovery_beacon_enabled);
}

static void nanUpdateFaw(struct ADAPTER *prAdapter,
			 uint8_t *peerNMI,
			 struct IE_NAN_ASCC_FAW *faw)
{
	struct IE_NAN_ASCC_FAW *pFaw = NULL;
	enum _ENUM_BAND_INDEX_T band;
	const char *type = NULL;

	band = getScheduleEntryBand(faw->schedule_category);
	if (band >= ARRAY_SIZE(gAsccRecord.arFcEntry))
		return;

	gAsccRecord.custom_schedule_entry |= BIT(faw->schedule_category);
	DBGLOG(NAN, INFO, "set %u, custom_schedule_entry=0x%08x\n",
	       faw->schedule_category,
	       gAsccRecord.custom_schedule_entry);

	if (isAisScheduleEntry(faw->schedule_category)) {
		/* TODO: write default if not set */
		pFaw = &gAsccRecord.arApEntry[band];
		type = "AP";
	} else if (isNdpScheduleEntry(faw->schedule_category)) {
		uint32_t r;
		struct _NAN_NDL_INSTANCE_T *prNDL;
		uint8_t aucClusterId[MAC_ADDR_LEN] = {0};
		uint8_t ucIndex = INVALID_SLOT_INDEX;

		r = nanDevGetClusterId(prAdapter, aucClusterId);
		if (r == WLAN_STATUS_SUCCESS &&
		    EQUAL_MAC_ADDR(peerNMI, aucClusterId)) {
			ucIndex = NAN_MAX_CONN_CFG;
		} else {
			prNDL = nanDataUtilSearchNdlByMac(prAdapter, peerNMI);
			if (prNDL)
				ucIndex = prNDL->ucIndex;
		}

		if (ucIndex != INVALID_SLOT_INDEX) {
			gAsccRecord.u4NdpEntryConnSetBitmap |= BIT(ucIndex);
			pFaw = &gAsccRecord.arNdpEntry[band][ucIndex];
			type = "NDP";
		}
	} else if (isNanSocialScheduleEntry(faw->schedule_category)) { /* FC */
		pFaw = &gAsccRecord.arFcEntry[band];
		type = "FC";
	}

	if (!pFaw)
		return;

	kalMemCopy(pFaw, faw, sizeof(*faw));

	DBGLOG(NAN, INFO, "Update %s FAW [%u] ch=%u, bitmap=0x%08x\n",
	       type, band, pFaw->ucOpChannel, pFaw->u4Bitmap);
}

static void nanExcludeAisSlots(struct ADAPTER *prAdapter,
			       enum ENUM_BAND eCustBand, uint32_t *u4Bitmap)
{
	enum ENUM_BAND eAisBand; /* 1-index */
	union _NAN_BAND_CHNL_CTRL rChnlInfo;
	uint32_t u4SlotBitmap;
	uint8_t ucPhyTypeSet;

	if (nanSchedGetAisChnlUsage(prAdapter, &rChnlInfo, &u4SlotBitmap,
				    &ucPhyTypeSet) !=
	    WLAN_STATUS_SUCCESS)
		return;

	eAisBand = nanRegGetNanChnlBand(rChnlInfo);
	DBGLOG(NAN, INFO, "Check band:%u vs. AIS: chnl:%u b:%u butmap:0x%08x\n",
	       eCustBand,
	       rChnlInfo.rChannel.u4PrimaryChnl, eAisBand, u4SlotBitmap);

	if (nanGetTimelineNum() == 1 ||
	    (eAisBand == eCustBand ||
	     (eAisBand == BAND_5G && eCustBand == BAND_6G ||
	      eAisBand == BAND_6G && eCustBand == BAND_5G))) {
		DBGLOG(NAN, INFO, "Update 0x%08x => 0x%08x\n", *u4Bitmap,
		       *u4Bitmap & ~u4SlotBitmap);
		*u4Bitmap &= ~u4SlotBitmap;
	}
}

static void nanUpdateUsd(struct ADAPTER *prAdapter, struct IE_NAN_ASCC_USD *usd)
{
	struct IE_NAN_ASCC_USD *pUsd;
	enum _ENUM_BAND_INDEX_T band;

	band = getScheduleEntryBand(usd->schedule_category);
	if (band >= ARRAY_SIZE(gAsccRecord.arUsdEntry))
		return;

	gAsccRecord.custom_schedule_entry |= BIT(usd->schedule_category);
	DBGLOG(NAN, INFO, "set %u, custom_schedule_entry=0x%08x\n",
	       usd->schedule_category,
	       gAsccRecord.custom_schedule_entry);

	nanExcludeAisSlots(prAdapter, bandTable[band], &usd->u4Bitmap);
	pUsd = &gAsccRecord.arUsdEntry[band];
	kalMemCopy(pUsd, usd, sizeof(*usd));

	DBGLOG(NAN, INFO,
	       "Update USD[%u] ch=%u, timeout=%u, num=%u, interval=%u, bitmap=0x%08x\n",
	       band, pUsd->ucOpChannel, pUsd->ucUsdTimeout,
	       pUsd->number_unsoliciated_sdf,
	       pUsd->usd_interval, pUsd->u4Bitmap);
}

static void nanUpdateSd(struct ADAPTER *prAdapter, struct IE_NAN_ASCC_SD *sd)
{
	struct IE_NAN_ASCC_SD *pSd;
	enum _ENUM_BAND_INDEX_T band;

	band = getScheduleEntryBand(sd->schedule_category);
	if (band >= ARRAY_SIZE(gAsccRecord.arSdEntry))
		return;

	gAsccRecord.custom_schedule_entry |= BIT(sd->schedule_category);
	DBGLOG(NAN, INFO, "set %u, custom_schedule_entry=0x%08x\n",
	       sd->schedule_category,
	       gAsccRecord.custom_schedule_entry);

	nanExcludeAisSlots(prAdapter, bandTable[band], &sd->u4Bitmap);
	pSd = &gAsccRecord.arSdEntry[band];
	kalMemCopy(pSd, sd, sizeof(*sd));

	DBGLOG(NAN, INFO,
	       "Update SD[%u] ch=%u, timeout=%u, num=%u, interval=%u, bitmap=0x%08x\n",
	       band, pSd->ucOpChannel, pSd->ucSdTimeout,
	       pSd->number_unsoliciated_sdf,
	       pSd->sd_interval, pSd->u4Bitmap);
}

static void nanUpdateUlw(struct IE_NAN_ASCC_ULW *ulw)
{
	struct IE_NAN_ASCC_ULW *pUlw;
	enum _ENUM_BAND_INDEX_T band;

	band = getScheduleEntryBand(ulw->schedule_category);
	if (band >= ARRAY_SIZE(gAsccRecord.arUlwEntry))
		return;

	gAsccRecord.custom_schedule_entry |= BIT(ulw->schedule_category);
	DBGLOG(NAN, INFO, "set %u, custom_schedule_entry=0x%08x\n",
	       ulw->schedule_category,
	       gAsccRecord.custom_schedule_entry);

	pUlw = &gAsccRecord.arUlwEntry[band];
	kalMemCopy(pUlw, ulw, sizeof(*ulw));

	DBGLOG(NAN, INFO,
	       "Update ULW[%u] start=%u, duration=%u, period=%u, countdown=%u, channel=%u, persent=%u\n",
	       band, pUlw->u4StartingTime, pUlw->u4Duration, pUlw->u4Period,
	       pUlw->u4Countdown, pUlw->ucOpChannel, pUlw->u2NanPresent);
}

static void nanUpdateDiscovery(struct IE_NAN_ASCC_DISCOVERY *disc)
{
	struct IE_NAN_ASCC_DISCOVERY *pDisc;
	enum _ENUM_BAND_INDEX_T band;

	band = getScheduleEntryBand(disc->schedule_category);
	if (band >= ARRAY_SIZE(gAsccRecord.arUlwEntry))
		return;

	gAsccRecord.custom_schedule_entry |= BIT(disc->schedule_category);
	DBGLOG(NAN, INFO, "set %u, custom_schedule_entry=0x%08x\n",
	       disc->schedule_category,
	       gAsccRecord.custom_schedule_entry);

	pDisc = &gAsccRecord.arDiscoveryEntry[band];
	kalMemCopy(pDisc, disc, sizeof(*disc));

	DBGLOG(NAN, INFO,
	       "Update Discovery[%u] mscp_interval=%u\n",
	       band, pDisc->ucMscpInterval);
}

static uint32_t nanDwEntryHandler(struct ADAPTER *prAdapter,
				  void *cmd,
				  const uint8_t *buf,
				  u_int8_t update)
{
	struct IE_NAN_ASCC_DW *dw;
	enum _ENUM_BAND_INDEX_T band;
	uint32_t interval;

	DBGLOG(NAN, INFO, "Parsing DW schedule entry, consuming %zu bytes\n",
	       sizeof(struct IE_NAN_ASCC_DW));
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_ASCC_DW));

	dw = (struct IE_NAN_ASCC_DW *)buf;

	DBGLOG(NAN, INFO, "Schedule category: %u (%s)\n",
	       dw->schedule_category,
	       getScheduleEntryString(dw->schedule_category));
	DBGLOG(NAN, INFO, "Operation: %u (%s)\n",
	       dw->operation,
	       operation_str(dw->operation));
	DBGLOG(NAN, INFO, "Start offset: %u (%u TU)\n",
	       dw->start_offset,
	       dw->start_offset * 16); /* TODO */
	DBGLOG(NAN, INFO, "Channel: %u\n",
	       dw->ucOpChannel);
	DBGLOG(NAN, INFO, "DW wakeup interval: %u, (%u TUs)\n",
	       dw->dw_wakeup_interval,
	       calculate_dw_wakeup_interval(dw->dw_wakeup_interval));
	DBGLOG(NAN, INFO, "Discovery beacon enabled: %u\n",
	       dw->discovery_beacon_enabled);

	interval = calculate_dw_wakeup_interval(dw->dw_wakeup_interval);
	band = getScheduleEntryBand(dw->schedule_category);
	/* TODO: currently only take interval into account */
	if (interval <= NAN_TOTAL_SLOT_WINDOWS * NAN_SLOT_INTERVAL) {
		if (update)
			nanUpdateDw(dw);
	} else {
		DBGLOG(NAN, WARN, "interval=%u > bound %u\n",
		       interval,
		       NAN_TOTAL_SLOT_WINDOWS * NAN_SLOT_INTERVAL);
	}

	return sizeof(struct IE_NAN_ASCC_DW);
}

static uint32_t nanFawEntryHandler(struct ADAPTER *prAdapter,
				   void *cmd,
				   const uint8_t *buf,
				   u_int8_t update)
{
	struct IE_NAN_ASCC_FAW *faw;
	enum _ENUM_BAND_INDEX_T band;
	uint32_t u4Bitmap;
	uint32_t category;
	uint8_t *peerNMI = NULL;

	DBGLOG(NAN, INFO, "Parsing FAW schedule entry, consuming %zu bytes\n",
	       sizeof(struct IE_NAN_ASCC_FAW));
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_ASCC_FAW));


	if (update) {
		struct IE_NAN_EXT_COMMON_HEADER *h = cmd;

		if (h && h->ucOui == NAN_ASCC_CMD_OUI) {
			struct IE_NAN_ASCC_CMD *ascc_cmd = cmd;

			peerNMI = ascc_cmd->aucDestNMIAddr;
		}
	}

	faw = (struct IE_NAN_ASCC_FAW *)buf;

	DBGLOG(NAN, INFO, "Schedule category: %u (%s)\n",
	       faw->schedule_category,
	       getScheduleEntryString(faw->schedule_category));
	DBGLOG(NAN, INFO, "Operation: %u (%s)\n",
	       faw->operation,
	       operation_str(faw->operation));
	DBGLOG(NAN, INFO, "Bitmap: %08x %02x-%02x-%02x-%02x\n",
	       faw->u4Bitmap,
	       faw->byte[0], faw->byte[1], faw->byte[2], faw->byte[3]);
	DBGLOG(NAN, INFO, "Channel: %u\n",
	       faw->ucOpChannel);

	band = getScheduleEntryBand(faw->schedule_category);
	u4Bitmap = faw->u4Bitmap;
	category = faw->schedule_category;
	if (isAisScheduleEntry(category)) {
		if (update) {
			struct _NAN_AIS_BITMAP *pAisSlots;

			pAisSlots = prAdapter->arNanAisSlots;
			/* TODO: htonl()? */
#if CFG_SUPPORT_WIFI_6G  == 1
			if (band >= NAN_EXT_NUM_3_BANDS) {
				DBGLOG(NAN, WARN, "Invalid band:%u\n", band);
				goto done;
			}
#else
			if (band >= NAN_EXT_NUM_2_BANDS) {
				DBGLOG(NAN, WARN, "Invalid band:%u\n", band);
				goto done;
			}
#endif
			if (!peerNMI) {
				DBGLOG(NAN, WARN, "Invalid peerNMI=%p\n",
				       peerNMI);
				goto done;
			}
			pAisSlots[band].fgCustSet = TRUE;
			pAisSlots[band].u4Bitmap = u4Bitmap;

			nanUpdateFaw(prAdapter, peerNMI, faw);
		}
	} else if (isNdpScheduleEntry(category)) {
		/* Fake Attribute in nanAsccParseScheduleEntry */
		if (update) {
			if (!peerNMI) {
				DBGLOG(NAN, WARN, "Invalid peerNMI=%p\n",
				       peerNMI);
				goto done;
			}
			nanUpdateFaw(prAdapter, peerNMI, faw);
		}
	} else if (isNanSocialScheduleEntry(category)) {
		/* FC */
		if (update) {
			if (!peerNMI) {
				DBGLOG(NAN, WARN, "Invalid peerNMI=%p\n",
				       peerNMI);
				goto done;
			}
			nanUpdateFaw(prAdapter, peerNMI, faw);
		}
	} else {
		DBGLOG(NAN, INFO, "schedule entry %u not handled\n",
		       category);
	}

done:
	return sizeof(struct IE_NAN_ASCC_FAW);
}

/**
 * Table 8. USD interval field
 * 0: 256 TU
 * 1: 512 TU
 * 2: 1024 TU
 * 3: 2048 TU
 * 4: 4096 TU
 * 5: 8192 TU
 * 6~14: reserved
 * 15: Do not wake up at DW
 */
static uint32_t calculate_usd_interval(uint8_t usd_interval)
{
	if (usd_interval < 6) /* 512 * 2 ^ (n-1) */
		return (256U << usd_interval);
	else /* Not mentioned in the standard, assume Reserved */
		return 0;
}

static uint32_t nanUsdEntryHandler(struct ADAPTER *prAdapter,
				   void *cmd,
				   const uint8_t *buf,
				   u_int8_t update)
{
	struct IE_NAN_ASCC_USD *usd;
	enum _ENUM_BAND_INDEX_T band;

	DBGLOG(NAN, INFO, "Parsing USD schedule entry, consuming %zu bytes\n",
	       sizeof(struct IE_NAN_ASCC_USD));
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_ASCC_USD));

	usd = (struct IE_NAN_ASCC_USD *)buf;

	DBGLOG(NAN, INFO, "Schedule category: %u (%s)\n",
	       usd->schedule_category,
	       getScheduleEntryString(usd->schedule_category));
	DBGLOG(NAN, INFO, "Operation: %u (%s)\n",
	       usd->operation,
	       operation_str(usd->operation));
	DBGLOG(NAN, INFO, "Bitmap: %08x %02x-%02x-%02x-%02x\n",
	       usd->u4Bitmap,
	       usd->byte[0], usd->byte[1], usd->byte[2], usd->byte[3]);
	DBGLOG(NAN, INFO, "Channel: %u\n", usd->ucOpChannel);
	DBGLOG(NAN, INFO, "USD timeout: %u\n", usd->ucUsdTimeout);
	DBGLOG(NAN, INFO, "Number of unsolicited SDF: %u\n",
	       usd->number_unsoliciated_sdf);
	DBGLOG(NAN, INFO, "USD interval: %u (%u TU)\n",
	       usd->usd_interval,
	       calculate_usd_interval(usd->usd_interval));

	band = getScheduleEntryBand(usd->schedule_category);

	if (update)
		nanUpdateUsd(prAdapter, usd);

	return sizeof(struct IE_NAN_ASCC_USD);
}

/**
 * Table 9. SD interval field
 * 0: 256 TU
 * 1: 512 TU
 * 2: 1024 TU
 * 3: 2048 TU
 * 4: 4096 TU
 * 5: 8192 TU
 * 6~14: reserved
 * 15: Do not wake up at DW
 */
static uint32_t calculate_sd_interval(uint8_t sd_interval)
{
	if (sd_interval < 6) /* 512 * 2 ^ (n-1) */
		return (256U << sd_interval);
	else /* Not mentioned in the standard, assume Reserved */
		return 0;
}

static uint32_t nanSdEntryHandler(struct ADAPTER *prAdapter,
				  void *cmd,
				  const uint8_t *buf,
				  u_int8_t update)
{
	struct IE_NAN_ASCC_SD *sd;
	enum _ENUM_BAND_INDEX_T band;

	DBGLOG(NAN, INFO, "Parsing SD schedule entry, consuming %zu bytes\n",
	       sizeof(struct IE_NAN_ASCC_SD));
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_ASCC_SD));

	sd = (struct IE_NAN_ASCC_SD *)buf;

	DBGLOG(NAN, INFO, "Schedule category: %u (%s)\n",
	       sd->schedule_category,
	       getScheduleEntryString(sd->schedule_category));
	DBGLOG(NAN, INFO, "Operation: %u (%s)\n",
	       sd->operation,
	       operation_str(sd->operation));
	DBGLOG(NAN, INFO, "Bitmap: %08x %02x-%02x-%02x-%02x\n",
	       sd->u4Bitmap,
	       sd->byte[0], sd->byte[1], sd->byte[2], sd->byte[3]);
	DBGLOG(NAN, INFO, "Channel: %u\n", sd->ucOpChannel);
	DBGLOG(NAN, INFO, "Number of unsolicited SDF: %u\n",
	       sd->number_unsoliciated_sdf);
	DBGLOG(NAN, INFO, "SD interval: %u (%u TU)\n",
	       sd->sd_interval,
	       calculate_sd_interval(sd->sd_interval));
	DBGLOG(NAN, INFO, "SD timeout: %u\n", sd->ucSdTimeout);

	band = getScheduleEntryBand(sd->schedule_category);

	if (update)
		nanUpdateSd(prAdapter, sd);

	return sizeof(struct IE_NAN_ASCC_SD);
}

static uint32_t nanUlwEntryHandler(struct ADAPTER *prAdapter,
				   void *cmd,
				   const uint8_t *buf,
				   u_int8_t update)
{
	struct IE_NAN_ASCC_ULW *ulw;

	DBGLOG(NAN, INFO, "Parsing ULW schedule entry, consuming %zu bytes\n",
	       sizeof(struct IE_NAN_ASCC_ULW));
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_ASCC_ULW));

	ulw = (struct IE_NAN_ASCC_ULW *)buf;

	DBGLOG(NAN, INFO, "Schedule category: %u (%s)\n",
	       ulw->schedule_category,
	       getScheduleEntryString(ulw->schedule_category));
	DBGLOG(NAN, INFO, "Operation: %u (%s)\n",
	       ulw->operation,
	       operation_str(ulw->operation));
	DBGLOG(NAN, INFO, "Starting time: %u\n",
	       ulw->u4StartingTime);
	DBGLOG(NAN, INFO, "Duration: %u us\n",
	       ulw->u4Duration);
	DBGLOG(NAN, INFO, "Period: %u us\n",
	       ulw->u4Period);
	DBGLOG(NAN, INFO, "Countdown: %u\n",
	       ulw->u4Countdown);
	DBGLOG(NAN, INFO, "Channel: %u\n",
	       ulw->ucOpChannel);

	/* TODO: set timline to exclude slots? */

	nanUpdateUlw(ulw);
	return sizeof(struct IE_NAN_ASCC_ULW);
}

static uint32_t nanDiscoveryEntryHandler(struct ADAPTER *prAdapter,
					 void *cmd,
					 const uint8_t *buf,
					 u_int8_t update)
{
	struct IE_NAN_ASCC_DISCOVERY *discovery;

	DBGLOG(NAN, INFO,
	       "Parsing Discovery schedule entry, consuming %zu bytes\n",
	       sizeof(struct IE_NAN_ASCC_DISCOVERY));
	DBGLOG_HEX(NAN, INFO, buf, sizeof(struct IE_NAN_ASCC_DISCOVERY));

	discovery = (struct IE_NAN_ASCC_DISCOVERY *)buf;

	DBGLOG(NAN, INFO, "Schedule category: %u (%s)\n",
	       discovery->schedule_category,
	       getScheduleEntryString(discovery->schedule_category));
	DBGLOG(NAN, INFO, "Operation: %u (%s)\n",
	       discovery->operation,
	       operation_str(discovery->operation));
	DBGLOG(NAN, INFO, "MSCP interval: %u\n",
	       discovery->ucMscpInterval);

	/* TODO: do something? */
	nanUpdateDiscovery(discovery);

	return sizeof(struct IE_NAN_ASCC_DISCOVERY);
}

static u_int8_t isAisCategorySameBand(union _NAN_BAND_CHNL_CTRL *rNanChnlInfo,
				      uint32_t schedule_category)
{
	enum ENUM_BAND band1;
	enum _ENUM_BAND_INDEX_T band2;

	band1 = nanRegGetNanChnlBand(*rNanChnlInfo);
	band2 = getScheduleEntryBand(schedule_category);

	return (band1 == BAND_2G4 && band2 == NAN_EXT_BAND_2G4 ||
#if (CFG_SUPPORT_WIFI_6G == 1)
		band1 == BAND_6G && band2 == NAN_EXT_BAND_6G ||
#endif
		band1 == BAND_5G && band2 == NAN_EXT_BAND_2G4);
}

/**
 * nanGetScheduleEntry() - Get schedule entries from AScC storage
 * @prAdapter: pointer to adapter
 * @u4CategoryBitmap: use the bitmap provided to collect the entries.
 *                    Can use gAsccRecord.custom_schedule_entry to use the
 *                    configured set bits.
 * @prResponse: pointer to response event as destination buffer for adding
 *		schedule entries.
 *		To get the size of required buffer without appending data,
 *		pass this pointer with NULL.
 *
 * Context:
 * Usage 1: get the required size of schedule entries with prResponse = NULL;
 * Usage 2: append the schedule entries
 *
 * Return: total size of schedule entries
 */
static uint32_t nanGetScheduleEntry(struct ADAPTER *prAdapter,
			uint32_t u4CategoryBitmap,
			/* also as flag to write to schedule entry */
			struct IE_NAN_ASCC_EVENT *prResponse)
{
	uint32_t offset = 0;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(nanExtSchedEntry); i++) {
		if (!(u4CategoryBitmap & BIT(i)) &&
		    !isAisScheduleEntry(i))
			continue;

		if (isAisScheduleEntry(i)) {
			union _NAN_BAND_CHNL_CTRL rChnlInfo = {0};
			uint32_t u4SlotBitmap;
			uint8_t ucPhyTypeSet;
			struct IE_NAN_ASCC_FAW *faw;
			enum _ENUM_BAND_INDEX_T b;

			nanSchedGetAisChnlUsage(prAdapter,
						&rChnlInfo, &u4SlotBitmap,
						&ucPhyTypeSet);
			if (rChnlInfo.rChannel.u4PrimaryChnl == 0 ||
			    !isAisCategorySameBand(&rChnlInfo, i)) {
				DBGLOG(NAN, WARN,
				       "Skip, not same band AIS r.chnl=%u, i=%u",
				       rChnlInfo.rChannel.u4PrimaryChnl, i);
				continue;
			}

			b = getScheduleEntryBand(i);
#if CFG_SUPPORT_WIFI_6G  == 1
			if (unlikely(b >= NAN_EXT_NUM_3_BANDS)) {
				DBGLOG(NAN, WARN, "Invalid band:%u\n", b);
				continue;
			}
#else
			if (unlikely(b >= NAN_EXT_NUM_2_BANDS)) {
				DBGLOG(NAN, WARN, "Invalid band:%u\n", b);
				continue;
			}
#endif

			DBGLOG(NAN, WARN, "Set AIS r.chnl=%u, set band=%u",
			       rChnlInfo.rChannel.u4PrimaryChnl, b);
			if (prResponse) {
				faw = (struct IE_NAN_ASCC_FAW *)
						(prResponse->aucData + offset);
				faw->schedule_category = i;
				faw->operation = NAN_SCHEDULE_ENTRY_NOT_CHANGE;
				faw->u4Bitmap =
					prAdapter->arNanAisSlots[b].u4Bitmap;
				faw->ucOpChannel =
					rChnlInfo.rChannel.u4PrimaryChnl;
			}
		} else if (isNdpScheduleEntry(i)) {
			if (prResponse) {
				struct _NAN_NDL_INSTANCE_T *prNDL;
				struct IE_NAN_ASCC_FAW *faw;

				prNDL = nanDataUtilSearchNdlByMac(prAdapter,
						prResponse->aucPeerNMIAddr);
				if (!prNDL) {
					DBGLOG(NAN, WARN,
					   "MAC " MACSTR " not found\n",
					   MAC2STR(prResponse->aucPeerNMIAddr));
					continue;
				}
				faw = getScheduleEntryPtr(i);
				DBGLOG(NAN, INFO, "category=%u, %p", i, faw);
				DBGLOG_HEX(NAN, INFO, faw,
					   sizeof(struct IE_NAN_ASCC_FAW
						  [NAN_MAX_SUPPORT_NDL_NUM]));
				faw = &faw[prNDL->ucIndex];
				DBGLOG(NAN, INFO, "category=%u, %p", i, faw);
				DBGLOG_HEX(NAN, INFO, faw,
					   sizeof(struct IE_NAN_ASCC_FAW));

				kalMemCopy(prResponse->aucData + offset,
					   faw, getScheduleEntrySize(i));
			}
		} else {
			if (prResponse) {
				kalMemCopy(prResponse->aucData + offset,
					   getScheduleEntryPtr(i),
					   getScheduleEntrySize(i));
			}
		}

		if (prResponse)
			prResponse->u4Category |= BIT(i);

		offset += getScheduleEntrySize(i);
		DBGLOG(NAN, INFO, "Append schedule entry %u(%s), offset=%u",
		       i, getScheduleEntryString(i), offset);
	}
	DBGLOG(NAN, INFO, "all schedule entry size=%u\n", offset);

	return offset;
}

uint32_t nanParseScheduleEntry(struct ADAPTER *prAdapter,
			       void *cmd,
			       const uint8_t *buf, uint32_t size,
			       u_int8_t update)
{
	struct IE_NAN_ASCC_SCHEDULE *ascc_schedule;
	uint32_t offset = 0;
	uint32_t category;

	DBGLOG(NAN, INFO, "Enter %s, size=%u\n", __func__, size);
	DBGLOG_HEX(NAN, INFO, buf, size);

	while (offset < size) {
		ascc_schedule = (struct IE_NAN_ASCC_SCHEDULE *)&buf[offset];

		category = ascc_schedule->schedule_category;
		DBGLOG(NAN, INFO,
		       "Parsing schedule entry %u, remaining: %u\n",
		       category, size - offset);

		if (category >= ARRAY_SIZE(nanExtSchedEntry)) {
			DBGLOG(NAN, WARN, "Invalid schedule category %u\n",
			       category);
			break;

		}

		if (offset + getScheduleEntrySize(category) > size)
			break;

		if (!nanExtSchedEntry[category].handler)
			break;

		offset += nanExtSchedEntry[category].handler(prAdapter, cmd,
						buf + offset, update);
	}

	DBGLOG(NAN, INFO, "processed %u bytes", offset);
	return offset;
}

static void nanUpdateFastRecovery(struct ADAPTER *prAdapter,
				  struct IE_NAN_ASCC_CMD *cmd)
{
	struct _NAN_NDL_INSTANCE_T *prNDL;

	/* Get the pointer to prPeerSchRecord by NMI MAC through NDL */
	prNDL = nanDataUtilSearchNdlByMac(prAdapter, cmd->aucDestNMIAddr);
	if (!prNDL) {
		DBGLOG(NAN, WARN, "NDL for " MACSTR " not found\n",
		       MAC2STR(cmd->aucSrcNMIAddr));
	}

	if (!prNDL)
		return;

	if (prNDL) { /* Set/clear FR flags accordingly */
		if (cmd->usage == NAN_SCHEDULE_USAGE_ASCC_PS_FR ||
		    cmd->usage == NAN_SCHEDULE_USAGE_APNAN_PS_FR) {
			prNDL->u4FastRecoveryId = cmd->ucRequestId;
			prNDL->u4SetFastRecovery =
				(uint32_t)(kalGetBootTime() / USEC_PER_SEC);
			DBGLOG(NAN, INFO, "FR req=%u set %u time=%u\n",
			       cmd->ucRequestId, prNDL->ucIndex,
			       prNDL->u4SetFastRecovery);
		} else if (cmd->usage != NAN_SCHEDULE_USAGE_CHECK_SCHEDULE) {
			prNDL->u4SetFastRecovery = 0;
		}
	}
}


static void
nanUpdateCustomizedNdpSettings(struct _NAN_NDL_CUSTOMIZED_T *prNdlCustomized,
			       uint32_t u4Category, uint32_t u4SchIdx)
{
	enum _ENUM_BAND_INDEX_T band;
	enum NAN_SCHEDULE_CATEGORY entry_idx;
	struct IE_NAN_ASCC_FAW *prSchedEntryNdp;

	/* channel and bitmap saved in gAsccRecord.arNdpEntry[band][u4SchIdx]
	 * For each NDP category bitmap set,
	 * get the pointer to peer schedule entry,
	 * Set rCustomized[band].fgBitmapSet
	 * Save rCustomized[band].ucOpChannel
	 * Save rCustomized[band].u4Bitmap
	 * Call nanSchedPeerUpdateCommonFAW
	 */
	for (band = NAN_EXT_BAND_2G4, entry_idx = CATEGORY_NDP_FAW_2G;
	     band < ARRAY_SIZE(gAsccRecord.arNdpEntry);
	     band++, entry_idx += CATEGORY_NDP_FAW_5G -
				  CATEGORY_NDP_FAW_2G) {

		if ((u4Category & BIT(entry_idx)) == 0)
			continue;

		prSchedEntryNdp = getScheduleEntryPtr(entry_idx);
		/* &gAsccRecord.arNdpEntry[band] */
		if (!prSchedEntryNdp)
			continue;

		prNdlCustomized[band].fgBitmapSet = TRUE;
		prNdlCustomized[band].ucOpChannel =
			prSchedEntryNdp[u4SchIdx].ucOpChannel;
		prNdlCustomized[band].u4Bitmap =
			prSchedEntryNdp[u4SchIdx].u4Bitmap;

		DBGLOG(NAN, INFO,
		       "band=%u, u4SchIdx=%u, ucOpChannel=%u, u4Bitmap=%08x",
		       band, u4SchIdx,
		       prSchedEntryNdp[u4SchIdx].ucOpChannel,
		       prSchedEntryNdp[u4SchIdx].u4Bitmap);
	}
}

static void nanUpdateGlobalCustomizedNdp(struct ADAPTER *prAdapter,
					 struct IE_NAN_ASCC_CMD *cmd)
{
	struct _NAN_SCHEDULER_T *prScheduler;
	struct _NAN_NDL_CUSTOMIZED_T *prNdlCustomized;
	uint8_t aucClusterId[MAC_ADDR_LEN] = {0};
	uint32_t r;
	uint32_t u4SchIdx;

	r = nanDevGetClusterId(prAdapter, aucClusterId);
	if (r != WLAN_STATUS_SUCCESS)
		return;

	if (UNEQUAL_MAC_ADDR(cmd->aucDestNMIAddr, aucClusterId))
		return;

	prScheduler = nanGetScheduler(prAdapter);
	if (!prScheduler)
		return;

	DBGLOG(NAN, INFO, "Update NDP by Cluster ID " MACSTR "\n",
	       MAC2STR(aucClusterId));

	prNdlCustomized = prScheduler->arGlobalCustomized;
	nanUpdateCustomizedNdpSettings(prNdlCustomized, cmd->u4Category,
				       NAN_MAX_CONN_CFG);

	for (u4SchIdx = 0; u4SchIdx < NAN_MAX_CONN_CFG; u4SchIdx++)
		nanSchedPeerUpdateCommonFAW(prAdapter, u4SchIdx);

	/* Skip calling nanSchedReleaseUnusedCommitSlot to preserve
	 * local schedule
	 */

	nanSchedCmdUpdateAvailability(prAdapter);
}

static void nanUpdatePeerCustomizedNdp(struct ADAPTER *prAdapter,
				       struct IE_NAN_ASCC_CMD *cmd)
{
	struct _NAN_PEER_SCH_DESC_T *prPeerSchDesc;
	uint32_t u4SchIdx;
	struct _NAN_PEER_SCHEDULE_RECORD_T *prPeerSchRecord;
	struct _NAN_NDL_CUSTOMIZED_T *prNdlCustomized;

	prPeerSchDesc = nanSchedSearchPeerSchDescByNmi(prAdapter,
						       cmd->aucDestNMIAddr);

	if (!prPeerSchDesc) {
		DBGLOG(NAN, WARN, "PeerSchDesc for " MACSTR " not found\n",
		       MAC2STR(cmd->aucSrcNMIAddr));
		return;
	}

	u4SchIdx = prPeerSchDesc->u4SchIdx;
	if (unlikely(u4SchIdx >= NAN_MAX_CONN_CFG)) {
		DBGLOG(NAN, WARN, "u4SchIdx %u >= %u\n",
		       u4SchIdx, NAN_MAX_CONN_CFG);
		return;
	}

	DBGLOG(NAN, INFO, "Update NDP by Peer NMI " MACSTR ", SchIdx=%u\n",
	       MAC2STR(cmd->aucDestNMIAddr), u4SchIdx);

	prPeerSchRecord = nanSchedGetPeerSchRecord(prAdapter, u4SchIdx);
	prNdlCustomized = prPeerSchRecord->arCustomized;
	nanUpdateCustomizedNdpSettings(prNdlCustomized, cmd->u4Category,
				       u4SchIdx);

	nanSchedPeerUpdateCommonFAW(prAdapter, u4SchIdx);

	/* Skip calling nanSchedReleaseUnusedCommitSlot to preserve
	 * local schedule
	 */

	nanSchedCmdUpdateAvailability(prAdapter);
}

static void updatePeerNdpCustomizedSchedule(struct ADAPTER *prAdapter,
					    struct IE_NAN_ASCC_CMD *cmd)
{
	nanUpdateFastRecovery(prAdapter, cmd);

	nanUpdateGlobalCustomizedNdp(prAdapter, cmd); /* dest == cluster ID */
	nanUpdatePeerCustomizedNdp(prAdapter, cmd); /* dest == peer NMI */
}

uint32_t nanAsccParseScheduleEntry(struct ADAPTER *prAdapter,
			       struct IE_NAN_ASCC_CMD *cmd,
			       const uint8_t *buf, uint32_t size,
			       u_int8_t update)
{
	uint32_t offset = 0;

	DBGLOG(NAN, INFO, "Enter %s, size=%u\n", __func__, size);

	offset += nanParseScheduleEntry(prAdapter, cmd,
					buf, size, update);

	if (update && isAisScheduleEntrySet(cmd->u4Category)) {
		cnmAisInfraConnectNotify(prAdapter);
		nanSchedCommitNonNanChnlList(prAdapter); /* ais connected */
	}

	if (update && isNdpScheduleEntrySet(cmd->u4Category))
		updatePeerNdpCustomizedSchedule(prAdapter, cmd);

	DBGLOG(NAN, INFO, "processed %u bytes", offset);
	return offset;
}

const char *ascc_type_str(uint8_t i)
{
	static const char * const ascc_type_string[] = {
		[0] = "Request",
		[1] = "Response",
		[2] = "Confirm",
		[3] = "Reserved",
	};

	if (likely(i < ARRAY_SIZE(ascc_type_string)))
		return ascc_type_string[i];

	return "Invalid";
}

const char *ascc_status_str(uint8_t i)
{
	/* enum NAN_ASCC_STATUS */
	static const char * const status_string[] = {
		[0] = "NAN_ASCC_STATUS_SUCCESS",
		[1] = "NAN_ASCC_STATUS_SUCCESS_BUT_CHANGED",
		[2] = "NAN_ASCC_STATUS_FAIL",
		[3] = "NAN_ASCC_STATUS_ASYNC_EVENT",
	};
	if (likely(i < ARRAY_SIZE(status_string)))
		return status_string[i];

	return "Invalid";
}

static const char *ascc_reason_str(uint8_t type, uint8_t status, uint8_t reason)
{
	/* enum NAN_ASCC_REASON */
	static const char * const reason_string_response[] = {
		[0] = "REASON_SUCCESS",
		[1] = "REASON_OVERWRITE_FAILED",
		[2] = "REASON_MODIFY_PARTLY",
		[3] = "REASON_ASCC_PS_FAILED",
		[4] = "REASON_APNAN_PS_FAILED",
		[5 ... 13] = "",
		[14] = "REASON_COMMAND_PARSING_ERROR",
		[15] = "REASON_UNDEFINED_FAILURE",
	};

	static const char * const reason_string_async[] = {
		[0] = "REASON_FIRMWARE_SCAN_STARTED",
		[1] = "REASON_FRAMEWORK_SCAN_STARTED",
		[2] = "REASON_NAN_PASSIVE_SCAN_STARTED",
		[3] = "REASON_SCHED_NEGO_COMPLETED_NDP",
		[4] = "REASON_SCHED_NEGO_COMPLETED_OTHER",
		[5] = "REASON_SCHED_CHANGED_WITHOUT_NEGO",
		[6] = "REASON_SCHED_CHANGED_LEGACY_NAN",
	};

	static const char * const reason_string_confirm[] = {
		[1] = "REASON_PEER_SUCCESS_ASCC_PS",
		[2] = "REASON_PEER_SUCCESS_APNAN_PS",
		[3] = "REASON_PEER_SUCCESS_FR_GIVEN_SCHEDULE",
		[4] = "REASON_PEER_SUCCESS_FR_DIFFERENT_SCHEDULE",
	};

	if (type == NAN_ASCC_PACKET_RESPONSE) {
		if (status != NAN_ASCC_STATUS_ASYNC_EVENT) {
			if (likely(reason < ARRAY_SIZE(reason_string_response)))
				return reason_string_response[reason] ?
				       reason_string_response[reason] :
				       "Reserved";
		} else {
			if (likely(reason < ARRAY_SIZE(reason_string_async)))
				return reason_string_async[reason] ?
				       reason_string_async[reason] :
				       "Reserved";
		}
	} else if (type == NAN_ASCC_PACKET_CONFIRM) {
		if (likely(reason < ARRAY_SIZE(reason_string_confirm)))
			return reason_string_confirm[reason] ?
			       reason_string_confirm[reason] : "Reserved";
	}

	return "Invalid";
}

const char *scheduling_method_str(uint8_t i)
{
	static const char * const sheduling_method_string[] = {
		[0] = "Forced by framework",
		[1] = "Suggested by framework and manged using SDF",
		[2] = "Suggested by framework and manged by firmware",
		[3] = "Triggered and managed by firmware (Legacy operation)",
	};

	if (likely(i < ARRAY_SIZE(sheduling_method_string)))
		return sheduling_method_string[i];

	return "Invalid";
}

static const char *usage_str(uint8_t i)
{
	static const char * const usage_string[] = {
		[0] = "General",
		[1] = "CCM",
		[2] = "Fast Connect Control",
		[3] = "Fast Recovery",
		[4] = "AP-based NAN Synchronization",
		[5] = "P2P Concurrent Discovery",
		[6] = "AScC Power-save",
		[7] = "Adaptive NAN Scheduling",
		[8] = "AP-based NAN Power-save",
		[9] = "AScC PS with FR",
		[10] = "AP-NAN PS with FR",
		[11] = "Current schedule check",
		[12] = "USD or SD schedule change",
		[13] = "NDP idle timer configuration",
		[14 ... 15] = "Reserved",
	};

	if (likely(i < ARRAY_SIZE(usage_string)))
		return usage_string[i];

	return "Invalid";
}

static const char *tx_start_time_str(uint8_t i)
{
	static const char * const tx_start_time_string[] = {
		[0] = "Start immediately",
		[1] = "After a DW",
		[2] = "After 2 DWs",
		[3 ... 7] = "Reserved",
	};

	if (likely(i < ARRAY_SIZE(tx_start_time_string)))
		return tx_start_time_string[i];

	return "Invalid";
}

static const char *band_pref_str(uint8_t i)
{
	static const char * const band_pref_string[] = {
		[0] = "2.4G",
		[1] = "5G",
		[2] = "6G",
		[3 ... 7] = "Reserved",
	};

	if (likely(i < ARRAY_SIZE(band_pref_string)))
		return band_pref_string[i];

	return "Invalid";
}

static void freePendingCmd(struct ADAPTER *prAdapter,
			   struct IE_NAN_ASCC_PENDING_CMD *prPendingCmd)
{
	DBGLOG(NAN, INFO, "Enter %s, free pending request %u\n",
	       __func__,
	       ((struct IE_NAN_ASCC_CMD *)(prPendingCmd->cmd))->ucRequestId);

	cnmMemFree(prAdapter, prPendingCmd);
}

static uint32_t savePendingCmd(struct ADAPTER *prAdapter,
		      struct IE_NAN_ASCC_CMD *cmd)
{
	struct IE_NAN_ASCC_PENDING_CMD *prPendingCmd;

	DBGLOG(NAN, INFO,
	       "Enter %s, req=%u method=%u usage=%u category=0x%08x\n",
	       __func__, cmd->ucRequestId, cmd->scheduling_method,
	       cmd->usage, cmd->u4Category);

	prPendingCmd = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				   sizeof(struct IE_NAN_ASCC_PENDING_CMD) +
				   sizeof(uint8_t) * EXT_MSG_SIZE(cmd));

	if (!prPendingCmd) {
		DBGLOG(NAN, ERROR,
		       "No memory, id=%u, method=%u, usage=%u, category=0x%08x\n",
		       cmd->ucRequestId, cmd->scheduling_method,
		       cmd->usage, cmd->u4Category);
		return WLAN_STATUS_FAILURE;
	}
	kalMemZero(prPendingCmd, sizeof(struct IE_NAN_ASCC_PENDING_CMD) +
				 sizeof(uint8_t) * EXT_MSG_SIZE(cmd));

	kalMemCopy(prPendingCmd->cmd, cmd, EXT_MSG_SIZE(cmd));
	if (LINK_IS_INVALID(&g_rPendingReqList))
		LINK_INITIALIZE(&g_rPendingReqList);

	LINK_INSERT_TAIL(&g_rPendingReqList, &prPendingCmd->rLinkEntry);
	DBGLOG(NAN, TRACE,
	       "Save cmd req=%u, category=0x%08x to pending at %p, size=%zu, link num=%u\n",
	       cmd->ucRequestId, cmd->u4Category,
	       prPendingCmd->cmd, EXT_MSG_SIZE(prPendingCmd->cmd),
	       g_rPendingReqList.u4NumElem);


	return WLAN_STATUS_SUCCESS;
}

static void dumpAsccCommand(struct IE_NAN_ASCC_CMD *c)
{
	char category_buf[128] = {0};

	DBGLOG_HEX(NAN, INFO, c, sizeof(struct IE_NAN_ASCC_CMD));

	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
	       c->ucNanOui, c->ucNanOui, oui_str(c->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %d\n", c->u2Length);
	DBGLOG(NAN, INFO, "v%d.%d\n", c->ucMajorVersion, c->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d\n", c->ucRequestId);
	DBGLOG(NAN, INFO, "Type: %d (%s)\n", c->type, ascc_type_str(c->type));
	DBGLOG(NAN, INFO, "Scheduling method: %d (%s)\n",
	       c->scheduling_method,
	       scheduling_method_str(c->scheduling_method));
	DBGLOG(NAN, INFO, "Usage: %d (%s)\n", c->usage, usage_str(c->usage));
	DBGLOG(NAN, INFO, "SrcNMI: " MACSTR "\n", MAC2STR(c->aucSrcNMIAddr));
	DBGLOG(NAN, INFO, "DestNMI: " MACSTR "\n", MAC2STR(c->aucDestNMIAddr));
	DBGLOG(NAN, INFO, "Confirm: %d\n",
	       c->schedule_confirm_handshake_required);
	DBGLOG(NAN, INFO, "Start: %d (%s)\n",
	       c->data_transmission_start_timing,
	       tx_start_time_str(c->data_transmission_start_timing));
	DBGLOG(NAN, INFO, "Band: %u (%s)\n", c->band_preference,
	       band_pref_str(c->band_preference));
	DBGLOG(NAN, INFO, "Idle Timer paused: %u\n", c->ndp_idle_timer_paused);
	DBGLOG(NAN, INFO, "Category: 0x%08x (%s)\n", c->u4Category,
	       schedule_category_included_str(c->u4Category, category_buf,
					      sizeof(category_buf)));
}

static void dumpAsccResponse(struct IE_NAN_ASCC_EVENT *r)
{
	char category_buf[512] = {0};

	DBGLOG(NAN, INFO, "OUI: %d(%02X) (%s)\n",
	       r->ucNanOui, r->ucNanOui, oui_str(r->ucNanOui));
	DBGLOG(NAN, INFO, "Length: %d\n", r->u2Length);
	DBGLOG(NAN, INFO, "v%d.%d\n", r->ucMajorVersion, r->ucMinorVersion);
	DBGLOG(NAN, INFO, "ReqId: %d\n", r->ucRequestId);
	DBGLOG(NAN, INFO, "Type: %d (%s)\n",
	       r->type, ascc_type_str(r->type));
	DBGLOG(NAN, INFO, "Status: %d (%s)\n",
	       r->status, ascc_status_str(r->status));
	DBGLOG(NAN, INFO, "Reason: %d (%s)\n",
	       r->reason_code,
	       ascc_reason_str(r->type, r->status, r->reason_code));
	DBGLOG(NAN, INFO, "Scheduling method: %d (%s)\n",
	       r->scheduling_method,
	       scheduling_method_str(r->scheduling_method));
	DBGLOG(NAN, INFO, "Usage: %d (%s)\n", r->usage, usage_str(r->usage));
	DBGLOG(NAN, INFO, "PeerNMI: " MACSTR "\n", MAC2STR(r->aucPeerNMIAddr));
	DBGLOG(NAN, INFO, "Category: 0x%08x (%s)\n", r->u4Category,
	       schedule_category_included_str(r->u4Category,
					      category_buf,
					      sizeof(category_buf)));
}

/* Logic implement according to Table 14 in V2.0
 * Schedule entries for Status 01 (success but changed) or 11 (asyn event)
 * For each adopted (modified) schedule, schedule entry shall be provided.
 * For asynchronous reason code 3, 4, 5, and 6, schedule entry shall be provided
 */
static u_int8_t nanAsccResponseNeedScheduleEntry(enum NAN_ASCC_STATUS status,
						 enum NAN_ASCC_REASON reason)
{
	return (status == NAN_ASCC_STATUS_SUCCESS_BUT_CHANGED ||
		(status == NAN_ASCC_STATUS_ASYNC_EVENT &&
		 (reason == NAN_ASCC_REASON_SCHED_NEGO_COMPLETED_NDP ||
		  reason == NAN_ASCC_REASON_SCHED_NEGO_COMPLETED_OTHER ||
		  reason == NAN_ASCC_REASON_SCHED_CHANGED_WITHOUT_NEGO ||
		  reason == NAN_ASCC_REASON_SCHED_CHANGED_LEGACY_NAN)));
}

/* Implement according to Table 14, 15 in V2.0 and
 * Reason 0: FW triggered scan started
 * Reason 1: Framework triggered scan started
 * Reason 2: Wi-Fi Aware Passive scan started
 */
u_int8_t nanAsccResponseNeedAsyncEvent(enum NAN_ASCC_STATUS status,
					enum NAN_ASCC_REASON reason)
{
	return (status == NAN_ASCC_STATUS_ASYNC_EVENT &&
		(reason == NAN_ASCC_REASON_SCHED_NEGO_COMPLETED_NDP ||
		 reason == NAN_ASCC_REASON_SCHED_NEGO_COMPLETED_OTHER ||
		 reason == NAN_ASCC_REASON_SCHED_CHANGED_WITHOUT_NEGO ||
		 reason == NAN_ASCC_REASON_SCHED_CHANGED_LEGACY_NAN));
}

/**
 * nanCopyPendingScheduleEntry() - Copy schedule entry from pending command
 * @prAdapter: pointer to adapter
 * @prResponse: pointer to response event as destination buffer for schedule
 *		entries
 * @prCmd: pointer to pending AScC command to access source buffer for schedule
 *		  entries
 *
 * Context: limited to the usage when the schedule entry to be replied is
 * identical to the ones stored in request command
 *
 * Return: size of copied size
 */
static uint32_t nanCopyPendingScheduleEntry(struct ADAPTER *prAdapter,
			struct IE_NAN_ASCC_EVENT *prResponse,
			struct IE_NAN_ASCC_CMD *prCmd)
{
	kalMemCopy(prResponse->aucData, prCmd->aucEntry,
		   ASCC_CMD_BODY_SIZE(prCmd));

	return ASCC_CMD_BODY_SIZE(prCmd);
}

/**
 * Compose AScC asynchrnouns event
 * TODO: just a draft, need to refine
 */
static uint32_t nanAddAsccAsyncEvent(struct ADAPTER *prAdapter,
				     struct NAN_ASCC_ASYNC_EVENT *prAsync,
				     enum NAN_ASCC_REASON reason)
{
	prAsync->ucEvent = reason;
	DBGLOG(NAN, INFO, "Set AsyncEvent: %u\n",
	       prAsync->ucEvent, &prAsync->ucEvent);

	return sizeof(struct NAN_ASCC_ASYNC_EVENT);
}

static uint32_t nanParseAsyncEvent(struct ADAPTER *prAdapter,
				   struct NAN_ASCC_ASYNC_EVENT *prAsync)
{
	DBGLOG(NAN, INFO, "AsyncEvent: %u\n",
	       prAsync->ucEvent, &prAsync->ucEvent);

	return 0;
}

/**
 * Compose AScC Response message
 */
static uint32_t nanComposeASCCResponse(struct ADAPTER *prAdapter,
				struct IE_NAN_ASCC_CMD *cmd,
				enum NAN_ASCC_PACKET_TYPE type,
				enum NAN_ASCC_STATUS status,
				enum NAN_ASCC_REASON reason)
{
	struct IE_NAN_ASCC_EVENT *prResponse;
	uint32_t entry_size = 0;
	uint32_t u4ScheduleEntrySize = 0;
	uint32_t u4AsyncEventSize = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	DBGLOG(NAN, INFO, "Enter %s, request=%u type=%u status=%u reason=%u\n",
	       __func__, cmd->ucRequestId, type, status, reason);


	/* determine required schedule entry size */
	if (cmd->usage == NAN_SCHEDULE_USAGE_CHECK_SCHEDULE) {
		/* collect all ever set schedule entries */
		u4ScheduleEntrySize = nanGetScheduleEntry(prAdapter,
					gAsccRecord.custom_schedule_entry,
					NULL);
	} else if (nanAsccResponseNeedScheduleEntry(status, reason)) {
		DBGLOG(NAN, INFO,
		       "Need schedule entry (status=%u, reason=%u)\n",
		       status, reason);
		/* copy the schedule entries from command */
		u4ScheduleEntrySize = ASCC_CMD_BODY_SIZE(cmd);
	}

	/* determine required async event size */
	if (nanAsccResponseNeedAsyncEvent(status, reason)) {
		DBGLOG(NAN, INFO, "Need async event\n");
		u4AsyncEventSize = sizeof(struct NAN_ASCC_ASYNC_EVENT);
	}

	/* Get a buffer to fill response */
	prResponse = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				 NAN_MAX_EXT_DATA_SIZE);
	if (!prResponse) {
		rStatus = WLAN_STATUS_FAILURE;
		goto done;
	}

	prResponse->ucNanOui = NAN_ASCC_CMD_OUI;
	prResponse->u2Length =
		sizeof(struct IE_NAN_ASCC_EVENT) - ASCC_EVENT_HDR_LEN +
		u4ScheduleEntrySize + u4AsyncEventSize;
	DBGLOG(NAN, TRACE, "u2Length = %u (%zu-%lu+%u+%u)\n",
	       prResponse->u2Length,
	       sizeof(struct IE_NAN_ASCC_EVENT), ASCC_EVENT_HDR_LEN,
	       u4ScheduleEntrySize, u4AsyncEventSize);
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
	prResponse->reason_code = reason;
	prResponse->scheduling_method = cmd->scheduling_method;
	prResponse->usage = cmd->usage;
	COPY_MAC_ADDR(prResponse->aucPeerNMIAddr, cmd->aucDestNMIAddr);
	prResponse->u4Category = cmd->u4Category;

	if (cmd->usage == NAN_SCHEDULE_USAGE_CHECK_SCHEDULE) {
		nanGetScheduleEntry(prAdapter,
				    gAsccRecord.custom_schedule_entry,
				    prResponse);
	} else if (nanAsccResponseNeedScheduleEntry(status, reason)) {
		nanCopyPendingScheduleEntry(prAdapter, prResponse, cmd);
	} else { /* no schedule entries replied */
		prResponse->u4Category = 0;
	}

	if (nanAsccResponseNeedAsyncEvent(status, reason))
		nanAddAsccAsyncEvent(prAdapter,
				(struct NAN_ASCC_ASYNC_EVENT *)
				&prResponse->aucData[u4ScheduleEntrySize],
				reason);

	/* log to verify the result */
	dumpAsccResponse(prResponse);

	if (prResponse->u4Category) {
		entry_size = nanParseScheduleEntry(prAdapter,
					NULL,
					prResponse->aucData,
					ASCC_EVENT_BODY_SIZE(prResponse),
					FALSE);
	}

	if (nanAsccResponseNeedAsyncEvent(prResponse->status,
					  prResponse->reason_code))
		nanParseAsyncEvent(prAdapter, (struct NAN_ASCC_ASYNC_EVENT *)
				      &prResponse->aucData[entry_size]);

	/* Reuse the buffer from passed in from HAL */
	DBGLOG(NAN, INFO, "Copy %zu bytes\n", EXT_MSG_SIZE(prResponse));
	kalMemCopy(cmd, prResponse, EXT_MSG_SIZE(prResponse));

done:
	if (prResponse)
		cnmMemFree(prAdapter, prResponse);

	return rStatus;
}

static uint32_t nanAsccResponse(struct ADAPTER *prAdapter,
				struct IE_NAN_ASCC_CMD *cmd,
				enum NAN_ASCC_STATUS status,
				enum NAN_ASCC_REASON reason)
{
	enum NAN_ASCC_PACKET_TYPE type = NAN_ASCC_PACKET_RESPONSE;

	if (cmd->usage == NAN_SCHEDULE_USAGE_CHECK_SCHEDULE) { /* case 7 */
		type = NAN_ASCC_PACKET_RESPONSE;
		status = NAN_ASCC_STATUS_SUCCESS;
		reason = NAN_ASCC_REASON_SUCCESS;
	}

	return nanComposeASCCResponse(prAdapter, cmd, type, status, reason);
}

/* Handle on any social timeline changed of add, modify, delete */
void nanReconfigureCustFaw(struct ADAPTER *prAdapter)
{
	enum _ENUM_BAND_INDEX_T band;
	enum NAN_SCHEDULE_CATEGORY entry_idx;
	uint32_t custom_schedule_entry =
		gAsccRecord.custom_schedule_entry & SOCIAL_ENTRY_BITMAP;
	struct IE_NAN_ASCC_FAW *faw;
	struct IE_NAN_ASCC_USD *usd;
	struct IE_NAN_ASCC_SD *sd;

	nanSchedNegoCustFawResetCmd(prAdapter);

	/* FC */
	for (band = NAN_EXT_BAND_2G4, entry_idx = CATEGORY_FC_2G;
	     band < ARRAY_SIZE(gAsccRecord.arFcEntry);
	     band++, entry_idx += CATEGORY_FC_5G - CATEGORY_FC_2G) {
		if ((custom_schedule_entry & BIT(entry_idx)) == 0)
			continue;

		faw = &gAsccRecord.arFcEntry[band];
		nanSchedNegoCustFawConfigCmd(prAdapter, faw->ucOpChannel,
					     bandTable[band], faw->u4Bitmap);
	}

	/* USD */
	for (band = NAN_EXT_BAND_2G4, entry_idx = CATEGORY_USD_2G;
	     band < ARRAY_SIZE(gAsccRecord.arUsdEntry);
	     band++, entry_idx += CATEGORY_USD_5G - CATEGORY_USD_2G) {
		if ((custom_schedule_entry & BIT(entry_idx)) == 0)
			continue;

		usd = &gAsccRecord.arUsdEntry[band];
		nanSchedNegoCustFawConfigCmd(prAdapter, usd->ucOpChannel,
					     bandTable[band], usd->u4Bitmap);
	}

	/* SD */
	for (band = NAN_EXT_BAND_2G4, entry_idx = CATEGORY_SD_2G;
	     band < ARRAY_SIZE(gAsccRecord.arSdEntry);
	     band++, entry_idx += CATEGORY_SD_5G - CATEGORY_SD_2G) {
		if ((custom_schedule_entry & BIT(entry_idx)) == 0)
			continue;

		sd = &gAsccRecord.arSdEntry[band];
		nanSchedNegoCustFawConfigCmd(prAdapter, sd->ucOpChannel,
					     bandTable[band], sd->u4Bitmap);
	}

	nanSchedNegoCustFawApplyCmd(prAdapter);
}

/**
 * nanProcessAsccCommand() - Process ASCC command from framework
 * @prAdapter: pointer to adapter
 * @buf: buffer holding command and to be filled with response message
 * @u4Size: buffer size of passsed-in buf
 *
 * Context:
 *   The response shall be filled back into the buf in binary format
 *   to be replied to framework,
 *   The binary array will be translated to hexadecimal string in
 *   nanExtParseCmd().
 *   The u4Size shall be checked when filling the response message.
 *
 * Return:
 * WLAN_STATUS_SUCCESS on success;
 * WLAN_STATUS_FAILURE on fail;
 * WLAN_STATUS_PENDING to wait for event from firmware and run OID complete
 */
uint32_t nanProcessAsccCommand(struct ADAPTER *prAdapter, const uint8_t *buf,
			       size_t u4Size)
{
	uint32_t r = WLAN_STATUS_SUCCESS;
	uint32_t offset = 0;
	struct IE_NAN_ASCC_CMD *c = (struct IE_NAN_ASCC_CMD *)buf;

	DBGLOG(NAN, INFO, "Enter %s, consuming %zu bytes\n",
	       __func__, sizeof(struct IE_NAN_ASCC_CMD));

	dumpAsccCommand(c);

	offset += OFFSET_OF(struct IE_NAN_ASCC_CMD, aucEntry);
	offset += nanAsccParseScheduleEntry(prAdapter, c, c->aucEntry,
					    ASCC_CMD_BODY_SIZE(c), TRUE);

	DBGLOG(NAN, INFO, "parsed, custom_schedule_entry=0x%08x\n",
	       gAsccRecord.custom_schedule_entry);

	if (g_ucNanIsOn) {
		DBGLOG(NAN, INFO, "Proceed to send command\n");
		if (c->usage != NAN_SCHEDULE_USAGE_CHECK_SCHEDULE) {
			/* pass whole command to sync settings */
			nanSchedUpdateCustomCommands(prAdapter, buf,
						     ASCC_CMD_SIZE(buf));
		}

		/* update timeline */
		if (isNanSocialScheduleEntrySet(c->u4Category) && g_ucNanIsOn)
			nanReconfigureCustFaw(prAdapter);

	} else { /* used later for sending response */
		DBGLOG(NAN, INFO, "Save command to pending list\n");
		r = savePendingCmd(prAdapter, c);
		if (r != WLAN_STATUS_SUCCESS)
			return WLAN_STATUS_FAILURE;
	}

	return r;
}

uint32_t nanProcessASCC(struct ADAPTER *prAdapter, const uint8_t *buf,
			size_t u4Size)
{
	uint32_t r = WLAN_STATUS_SUCCESS;
	enum NAN_ASCC_STATUS status = NAN_ASCC_STATUS_SUCCESS;
	enum NAN_ASCC_REASON reason = NAN_ASCC_REASON_SUCCESS;

	r = nanProcessAsccCommand(prAdapter, buf, u4Size);
	if (r != WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;

	r = nanAsccResponse(prAdapter, (struct IE_NAN_ASCC_CMD *)buf,
			    status, reason);

	return r;
}

static void nanAsccProcessAllPendingCommands(struct ADAPTER *prAdapter)
{
	struct LINK_ENTRY *prEntry;
	struct LINK_ENTRY *prNextEntry;
	struct IE_NAN_ASCC_PENDING_CMD *prPendingCmd;

	LINK_FOR_EACH_SAFE(prEntry, prNextEntry, &g_rPendingReqList) {
		if (prEntry == NULL)
			break;

		prPendingCmd = LINK_ENTRY(prEntry,
					  struct IE_NAN_ASCC_PENDING_CMD,
					  rLinkEntry);
		LINK_REMOVE_KNOWN_ENTRY(&g_rPendingReqList, prEntry);

		DBGLOG(NAN, INFO, "link num=%u\n",
		       g_rPendingReqList.u4NumElem);

		DBGLOG(NAN, TRACE, "Process cmd at %p, size=%zu\n",
		       prPendingCmd->cmd, EXT_MSG_SIZE(prPendingCmd->cmd));
		nanProcessAsccCommand(prAdapter, prPendingCmd->cmd,
				      EXT_MSG_SIZE(prPendingCmd->cmd));
		freePendingCmd(prAdapter, prPendingCmd);
	}
}

static void nanAsccFastConnect(struct ADAPTER *prAdapter)
{
	uint8_t *buf;
	size_t szSize = sizeof(struct IE_NAN_ASCC_CMD) +
		sizeof(struct IE_NAN_ASCC_FAW) * 2;
	struct IE_NAN_ASCC_CMD *cmd;
	struct IE_NAN_ASCC_FAW *prFc2G = NULL;
	struct IE_NAN_ASCC_FAW *prFc5G;

	if (!prAdapter->rWifiVar.fgNanAutoFC)
		return;

	szSize = sizeof(struct IE_NAN_ASCC_CMD) +
		sizeof(struct IE_NAN_ASCC_FAW);

	if (prAdapter->rWifiVar.fgNanAutoFC == 2)
		szSize += sizeof(struct IE_NAN_ASCC_FAW);

	buf = cnmMemAlloc(prAdapter, RAM_TYPE_BUF, szSize);
	if (!buf)
		return;

	kalMemZero(buf, szSize);

	cmd = (struct IE_NAN_ASCC_CMD *)buf;

	if (prAdapter->rWifiVar.fgNanAutoFC == 2)
		prFc2G = (struct IE_NAN_ASCC_FAW *)cmd->aucEntry;

	prFc5G = (struct IE_NAN_ASCC_FAW *)cmd->aucEntry;

	*cmd = (struct IE_NAN_ASCC_CMD) {
		.ucNanOui = NAN_ASCC_CMD_OUI,
		.u2Length = szSize - ASCC_CMD_HDR_LEN,
		.ucMajorVersion = 2,
		.ucMinorVersion = 1,
		.ucRequestId = 0,
		.type = NAN_ASCC_PACKET_REQUEST,
		.scheduling_method = NAN_SCHEDULE_METHOD_FRAMEWORK_FORCED,
		.usage = NAN_SCHEDULE_USAGE_FC,
		.u4Category = BIT(CATEGORY_FC_5G),
	};

	if (prAdapter->rWifiVar.fgNanAutoFC == 2) { /* 2G + 5G */
		cmd->u4Category |= BIT(CATEGORY_FC_2G);

		*prFc2G = (struct IE_NAN_ASCC_FAW) {
			.schedule_category = CATEGORY_FC_2G,
			.operation = NAN_SCHEDULE_ENTRY_ADD,
			.u4Bitmap = 0x00030002, /* #67 */ /* #68: 0x000f000e */
			.ucOpChannel = 6,
		};
		prFc5G = (struct IE_NAN_ASCC_FAW *)
			&cmd->aucEntry[sizeof(struct IE_NAN_ASCC_FAW)];
	}

	*prFc5G = (struct IE_NAN_ASCC_FAW) {
		.schedule_category = CATEGORY_FC_5G,
		.operation = NAN_SCHEDULE_ENTRY_ADD,
		.u4Bitmap = 0x00000e00,  /* #69 */ /* #70: 0x03003e00 */
		.ucOpChannel = 149,
	};

	nanProcessAsccCommand(prAdapter, buf, szSize);

	cnmMemFree(prAdapter, buf);
}

void nanAsccNanOnHandler(struct ADAPTER *prAdapter)
{
	nanSchedNegoCustFawResetCmd(prAdapter);
	nanAsccFastConnect(prAdapter);
	nanAsccProcessAllPendingCommands(prAdapter);

}

static void nanAsccResetSettings(struct ADAPTER *prAdapter)
{
	kalMemZero(&gAsccRecord, sizeof(gAsccRecord));
}

static void nanAsccClearPendingCommands(struct ADAPTER *prAdapter)
{
	struct LINK_ENTRY *prEntry;
	struct LINK_ENTRY *prNextEntry;
	struct IE_NAN_ASCC_PENDING_CMD *prPendingCmd;

	LINK_FOR_EACH_SAFE(prEntry, prNextEntry, &g_rPendingReqList) {
		if (prEntry == NULL)
			break;
		prPendingCmd = LINK_ENTRY(prEntry,
					  struct IE_NAN_ASCC_PENDING_CMD,
					  rLinkEntry);
		LINK_REMOVE_KNOWN_ENTRY(&g_rPendingReqList, prEntry);
		DBGLOG(NAN, TRACE, "Free cmd at %p, size=%zu\n",
		       prPendingCmd->cmd, EXT_MSG_SIZE(prPendingCmd->cmd));
		freePendingCmd(prAdapter, prPendingCmd);
	}
}

static void nanAsccReset(struct ADAPTER *prAdapter)
{
	nanAsccResetSettings(prAdapter);
	nanAsccClearPendingCommands(prAdapter);
}

void nanAsccNanOffHandler(struct ADAPTER *prAdapter)
{
	nanAsccReset(prAdapter);
}

/* currently not used */
struct IE_NAN_ASCC_PENDING_CMD *getPendingCommand(uint8_t ucRequestId)
{
	struct LINK_ENTRY *prEntry;
	struct IE_NAN_ASCC_PENDING_CMD *prPendingCmd;
	struct IE_NAN_ASCC_CMD *cmd;

	LINK_FOR_EACH(prEntry, &g_rPendingReqList) {
		prPendingCmd = LINK_ENTRY(prEntry,
					  struct IE_NAN_ASCC_PENDING_CMD,
					  rLinkEntry);
		cmd = (struct IE_NAN_ASCC_CMD *)prPendingCmd->cmd;
		if (cmd->ucRequestId == ucRequestId) {
			LINK_REMOVE_KNOWN_ENTRY(&g_rPendingReqList, prEntry);
			break;
		}
	}

	if (!prPendingCmd || cmd->ucRequestId != ucRequestId) {
		DBGLOG(NAN, INFO, "No pending request %u found\n", ucRequestId);
		return NULL;
	}

	DBGLOG(NAN, INFO,
	       "link num=%u, cmd->u4Category=0x%08x\n",
	       g_rPendingReqList.u4NumElem, cmd->u4Category);

	return prPendingCmd;
}

static void nanAsccResetNdlBitmap(struct ADAPTER *prAdapter,
				  uint32_t schedule_category)
{
	uint32_t u4SchIdx;
	struct _NAN_PEER_SCHEDULE_RECORD_T *prPeerSchRecord;
	enum _ENUM_BAND_INDEX_T band;

	band = getScheduleEntryBand(schedule_category);
#if CFG_SUPPORT_WIFI_6G  == 1
	if (band >= NAN_EXT_NUM_3_BANDS) {
		DBGLOG(NAN, WARN, "Invalid band:%u\n", band);
		return;
	}
#else
	if (band >= NAN_EXT_NUM_2_BANDS) {
		DBGLOG(NAN, WARN, "Invalid band:%u\n", band);
		return;
	}
#endif

	for (u4SchIdx = 0; u4SchIdx < NAN_MAX_CONN_CFG; u4SchIdx++) {
		prPeerSchRecord = nanSchedGetPeerSchRecord(prAdapter, u4SchIdx);
		if (!prPeerSchRecord->fgActive)
			continue;

		if (prPeerSchRecord->arCustomized[band].fgBitmapSet) {
			kalMemZero(&prPeerSchRecord->arCustomized[band],
				   sizeof(struct _NAN_NDL_CUSTOMIZED_T));
			nanSchedPeerUpdateCommonFAW(prAdapter, u4SchIdx);
		}
	}
}

/* u4SchIdx: prPeerSchDesc->u4SchIdx */
void nanExtClearCustomNdpFaw(uint8_t u4SchIdx)
{
	enum _ENUM_BAND_INDEX_T band;
	enum NAN_SCHEDULE_CATEGORY entry_idx;
	struct IE_NAN_ASCC_FAW *prSchedEntryNdp;

	if ((gAsccRecord.u4NdpEntryConnSetBitmap & BIT(u4SchIdx)) == 0)
		return;

	for (band = NAN_EXT_BAND_2G4, entry_idx = CATEGORY_NDP_FAW_2G;
	     band < ARRAY_SIZE(gAsccRecord.arNdpEntry);
	     band++, entry_idx += CATEGORY_NDP_FAW_5G -
				  CATEGORY_NDP_FAW_2G) {
		prSchedEntryNdp = getScheduleEntryPtr(entry_idx);
		if (!prSchedEntryNdp)
			continue;

		kalMemZero(&prSchedEntryNdp[u4SchIdx],
			   sizeof(prSchedEntryNdp[u4SchIdx]));
	}

	gAsccRecord.u4NdpEntryConnSetBitmap &= ~BIT(u4SchIdx);

	if (gAsccRecord.u4NdpEntryConnSetBitmap == 0)
		gAsccRecord.custom_schedule_entry &= ~NDP_ENTRY_BITMAP;
}

uint32_t nanAsccBackToNormal(struct ADAPTER *prAdapter)
{
	uint8_t buf[NAN_MAX_EXT_DATA_SIZE] = {};
	struct IE_NAN_ASCC_CMD *c = (struct IE_NAN_ASCC_CMD *)buf;
	size_t i;
	uint16_t *pu2Length;
	u_int8_t fgResetTimeline = FALSE;
	u_int8_t fgAisUpdated = FALSE;

	c->ucNanOui = NAN_ASCC_CMD_OUI;
	c->u2Length = sizeof(struct IE_NAN_ASCC_CMD) - ASCC_CMD_HDR_LEN;
	c->ucMajorVersion = 2;
	c->ucMinorVersion = 1;
	c->ucRequestId = 0;
	c->type = NAN_ASCC_PACKET_REQUEST;
	c->scheduling_method = NAN_SCHEDULE_METHOD_FRAMEWORK_FORCED;
	c->usage = NAN_SCHEDULE_USAGE_GENERAL;
	COPY_MAC_ADDR(c->aucDestNMIAddr, gAsccRecord.dst_mac);
	c->schedule_confirm_handshake_required = FALSE;
	c->data_transmission_start_timing = 0;
	c->band_preference = NAN_SCHEDULE_BAND_PREF_6G;
	c->ndp_idle_timer_paused = FALSE; /* NDP disconnect after timeout */
	c->u4Category = gAsccRecord.custom_schedule_entry;

	pu2Length = &c->u2Length;

	for (i = 0; i < sizeof(gAsccRecord.custom_schedule_entry) * CHAR_BIT;
	     i++) {
		if (BIT(i) & gAsccRecord.custom_schedule_entry) {
			if (isNanDwScheduleEntry(i)) { /* sync to FW */
				/* Collect schedule entry to pass to FW */
				kalMemCopy(&c->aucEntry[ASCC_CMD_SIZE(c)],
					   getScheduleEntryPtr(i),
					   getScheduleEntrySize(i));
				*pu2Length += getScheduleEntrySize(i);
			}

			if (isNanSocialScheduleEntry(i)) { /* sync to FW */
				/* Collect schedule entry to pass to FW */
				kalMemCopy(&c->aucEntry[ASCC_CMD_SIZE(c)],
					   getScheduleEntryPtr(i),
					   getScheduleEntrySize(i));
				*pu2Length += getScheduleEntrySize(i);
				fgResetTimeline = TRUE;
			}

			if (isNdpScheduleEntry(i)) { /* No need to sync to FW */
				/* Reset per-peer NDL settings in driver */
				nanAsccResetNdlBitmap(prAdapter, i);
			}

			if (isAisScheduleEntry(i)) { /* No need to sync to FW */
				struct _NAN_AIS_BITMAP *prAisSlots;
				enum _ENUM_BAND_INDEX_T band;

				/* Reset AIS bitmap settings in driver */
				prAisSlots = prAdapter->arNanAisSlots;
				band = getScheduleEntryBand(i);
#if CFG_SUPPORT_WIFI_6G  == 1
				if (band >= NAN_EXT_NUM_3_BANDS)
					continue;
#else
				if (band >= NAN_EXT_NUM_2_BANDS)
					continue;
#endif

				prAisSlots[band].fgCustSet = FALSE;
				nanUpdateAisBitmap(prAdapter, FALSE);
				fgAisUpdated = TRUE;
			}
			kalMemZero(getScheduleEntryPtr(i),
				   getScheduleEntrySize(i));
		}
	}

	if (fgResetTimeline) { /* TODO: specify FC, USD, SD? */
		/* Reset timeline in driver */
		nanSchedNegoCustFawResetCmd(prAdapter);
		nanSchedNegoCustFawApplyCmd(prAdapter);
	}
	if (fgAisUpdated) {
		cnmAisInfraConnectNotify(prAdapter);
		nanSchedCommitNonNanChnlList(prAdapter); /* ais connected */
	}

	/* pass command to FW to reset to default for DW and Social */
	nanSchedUpdateCustomCommands(prAdapter, buf,
				     ASCC_CMD_SIZE(buf));


	return WLAN_STATUS_SUCCESS;
}
#endif
