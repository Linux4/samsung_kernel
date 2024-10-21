/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_ADSDC_H_
#define _NAN_EXT_ADSDC_H_

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#if CFG_SUPPORT_NAN
/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
enum NAN_ADSDC_PACKET_TYPE {
	NAN_ADSDC_PACKET_REQUEST = 0,
	NAN_ADSDC_PACKET_REQUEST_WO_RESP = 1,
	NAN_ADSDC_PACKET_RESPONSE = 2,
	/* 3: RESERVED */
};

enum NAN_ADSDC_REASON {
	/* Type == 0b10 (Confirm) */
	NAN_ADSDC_REASON_SUCCESS = 0,
	NAN_ADSDC_REASON_FAIL_ROAMING_SCAN = 1,
	NAN_ADSDC_REASON_FAIL_OTHER_USE = 2,
	NAN_ADSDC_REASON_FAIL_MASTER_SELECTION = 3,
	NAN_ADSDC_REASON_FAIL_SERVICE_ROLE_SELECTION = 4,
	NAN_ADSDC_REASON_FAIL_2G_BEACON = 5,
	NAN_ADSDC_REASON_FAIL_5G_BEACON = 6,
	NAN_ADSDC_REASON_FAIL_NOT_SUPPORTED = 13,
	NAN_ADSDC_REASON_FAIL_PARSING_ERROR = 14,
	NAN_ADSDC_REASON_FAIL_UNDEFINED = 15,
};

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
struct IE_NAN_ADSDC_PENDING_CMD {
	struct LINK_ENTRY rLinkEntry;
	uint8_t ucNanOui;
	u8 cmd[NAN_MAX_EXT_DATA_SIZE];
};

/* [v2.1] Table 17. ADSDC Command */
struct IE_NAN_ADSDC_CMD {
	/* 0x40 = 64 */
	uint8_t ucNanOui; /* NAN_ADSDC_CMD_OUI */
	/* 0x0000 ~ 0xFFFF */
	uint16_t u2Length;
	/* 0x0 ~ 0xF */
	uint8_t ucMajorVersion;
	/* 0x0 ~ 0xF */
	uint8_t ucMinorVersion;
	/* 0x00 ~ 0xFF */
	uint8_t ucRequestId;
	/*
	 * BITS(6, 7): Type
	 * 00: Request
	 * 01: Request (Response not required)
	 * 10: Response
	 *
	 * BITS(4, 5): Channel quality report type
	 * 00: Received signal strength (dBm)
	 * 01: Quantized signal level
	 * 10: Signal to noise ratio (dB)
	 *
	 * BITS(2, 3): Determine the master role
	 * 00: Master
	 * 11: Non-master
	 *
	 * BITS(0, 1): Role selection for service discovery
	 * 00: Publisher using both types of solicited and
	 *	   unsolicited SDF publish messages
	 * 01: Publisher using solicited
	 *	   SDF publish message only
	 * 10: Active subscriber
	 * 11: Passive subscriber
	 */
	uint8_t cmd_type:2,
		ch_type:2,
		role:2,
		role_type:2;

	/*
	 * Service instance ID
	 * 0: Don't care
	 * 0x01~0xFF: Service instance ID to be assigned
	 */
	uint8_t ucServiceId;
	/*
	 * Schedule category included
	 * Bit 0: 2.4 GHz DW
	 * Bit 1: 2.4 GHz Fast Connect (FC)
	 * Bit 2: 2.4 GHz NDP (FAW)
	 * Bit 3: 5 GHz DW
	 * Bit 4: 5 GHz FC
	 * Bit 5: 5 GHz NDP (FAW)
	 * Bit 6: 6 GHz DW
	 * Bit 7: 6 GHz FC
	 * Bit 8: 6 GHz NDP (FAW)
	 * Bit 9: 2.4 GHz AP
	 * Bit 10: 5 GHz AP
	 * Bit 11: 6 GHz AP
	 * Bit 12: 2.4 GHz USD
	 * Bit 13: 5GHz USD
	 * Bit 14: 2.4 GHz SD
	 * Bit 15: 5 GHz SD
	 * Bit 16: Passive Scan
	 * Bit 17: FW triggered scan
	 * Bit 18: 2.4 GHz ULW
	 * Bit 19: 5 GHz ULW
	 * Bit 20: 6 GHz ULW
	 * Bit 21~31: Reserved
	 */
	uint32_t u4Category;
	/*
	 * Schedule entry
	 */
	uint8_t aucEntry[];

	/* struct TX_DISCOVERY_BEACON following  */
} __KAL_ATTRIB_PACKED__;

#define ADSDC_CMD_HDR_LEN OFFSET_OF(struct IE_NAN_ADSDC_CMD, ucMajorVersion)
#define ADSDC_CMD_LEN(fp) (((struct IE_NAN_ADSDC_CMD *)(fp))->u2Length)
#define ADSDC_CMD_SIZE(fp) (ADSDC_CMD_HDR_LEN + ADSDC_CMD_LEN(fp))
#define ADSDC_CMD_BODY_SIZE(fp)	\
	(ADSDC_CMD_SIZE(fp) - sizeof(struct IE_NAN_ADSDC_CMD))

struct TX_DISCOVERY_BEACON {
	uint8_t tx_discovery_beacon_2g:1,
		tx_discovery_beacon_5g:1,
		reserved:6;
} __KAL_ATTRIB_PACKED__;

/* [v2.1] Table 18. ADSDC TSF */
struct IE_NAN_ADSDC_TSF {
	/* 0x3F = 63 */
	uint8_t ucNanOui; /* NAN_ADSDC_TSF_OUI */
	/* 0x0000 ~ 0xFFFF */
	uint16_t u2Length;
	/* 0x0 ~ 0xF */
	uint8_t ucMajorVersion; /* TODO: field not defined in standard? */
	/* 0x0 ~ 0xF */
	uint8_t ucMinorVersion; /* TODO: field not defined in standard? */
	/* 0x00 ~ 0xFF */
	uint8_t ucRequestId; /* TODO: field not defined in standard? */
	/* 0 ~ FFFFFFFFFFFFFFFE */
	uint64_t u8Tsf;
	/* 0x00 ~ 0xFF */
	uint8_t ucRandFactor;
	/* 0x00 ~ 0xFF */
	uint8_t ucMasterPref;
} __KAL_ATTRIB_PACKED__;

/* [v2.1] Table 17. ADSDC Event */
struct IE_NAN_ADSDC_EVENT {
	/* 0x40 = 64 */
	uint8_t ucNanOui; /* NAN_ADSDC_CMD_OUI */
	/* 0x0000 ~ 0xFFFF */
	uint16_t u2Length;
	/* 0x0 ~ 0xF */
	uint8_t ucMajorVersion;
	/* 0x0 ~ 0xF */
	uint8_t ucMinorVersion;
	/* 0x00 ~ 0xFF */
	uint8_t ucRequestId;
	/*
	 * BITS(6, 7): Type
	 * 00: Request
	 * 01: Request (Response not required)
	 * 10: Response
	 *
	 * BITS(4, 5): Status
	 * 00: Success
	 * 01: Success but changed
	 * 10: Fail
	 *
	 * BITS(0, 3): Reason code
	 * For Type 10 (Response),
	 * Status 00~10
	 *
	 * 0d0: Success (Response)
	 * 0d1: The schedule should be used for roaming scan;
	 *		overwrite fail
	 * 0d2: The schedule should be used for other use:
	 *		modified partly
	 * 0d3: Master selection is not applied.
	 * 0d4: Service role selection is not applied.
	 * 0d5: 2.4 GHz Discovery beacon field is not applied.
	 * 0d6: 5 GHz Discovery beacon field is not applied.
	 * 0d7~0d13: Reserved
	 * 0d14: Command parsing error
	 * 0d15: Undefined failure
	 */
	uint8_t type:2,
		status:2,
		reason:4;

	/*
	 * BITS(6, 7): Scheduling control method
	 * 00: Forced by framework
	 * 01: Suggested by framework and managed using SDF
	 * 10: Suggested by framework and managed by firmware
	 * 11: Triggered and managed by firmware
	 *	  (Legacy operation)
	 *
	 * TBD?
	 */
	uint8_t scheduling_method:2,
		reserved:6;

	/*
	 * Schedule category included
	 * Bit 0: 2.4 GHz DW
	 * Bit 1: 2.4 GHz Fast Connect (FC)
	 * Bit 2: 2.4 GHz NDP (FAW)
	 * Bit 3: 5 GHz DW
	 * Bit 4: 5 GHz FC
	 * Bit 5: 5 GHz NDP (FAW)
	 * Bit 6: 6 GHz DW
	 * Bit 7: 6 GHz FC
	 * Bit 8: 6 GHz NDP (FAW)
	 * Bit 9: 2.4 GHz AP
	 * Bit 10: 5 GHz AP
	 * Bit 11: 6 GHz AP
	 * Bit 12: 2.4 GHz USD
	 * Bit 13: 5 GHz USD
	 * Bit 14: 2.4 GHz SD
	 * Bit 15: 5 GHz SD
	 * Bit 16: Passive scan
	 * Bit 17: FW triggered scan
	 * Bit 18: 2.4 GHz ULW
	 * Bit 19: 5 GHz ULW
	 * Bit 20: 6 GHz ULW
	 */
	uint32_t u4Category;
	/*
	 * Schedule entry for status01
	 */
	uint8_t aucEntry[];
} __KAL_ATTRIB_PACKED__;

#define ADSDC_EVENT_HDR_LEN OFFSET_OF(struct IE_NAN_ADSDC_EVENT, ucMajorVersion)
#define ADSDC_EVENT_LEN(fp) (((struct IE_NAN_ADSDC_EVENT *)(fp))->u2Length)
#define ADSDC_EVENT_SIZE(fp) (ADSDC_EVENT_HDR_LEN + ADSDC_EVENT_LEN(fp))
#define ADSDC_EVENT_BODY_SIZE(fp)	\
	(ADSDC_EVENT_SIZE(fp) - sizeof(struct IE_NAN_ADSDC_EVENT))

/*******************************************************************************
 *                            P U B L I C   D A T A
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
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

uint32_t nanProcessADSDC(struct ADAPTER *prAdapter, const uint8_t *buf,
			 size_t u4Size);

void nanAdsdcNanOnHandler(struct ADAPTER *prAdapter);
void nanAdsdcNanOffHandler(struct ADAPTER *prAdapter);

#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_EXT_ADSDC_H_ */

