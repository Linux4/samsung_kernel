/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_EHT_H_
#define _NAN_EXT_EHT_H_

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
enum NAN_EHT_PACKET_TYPE {
	NAN_EHT_PACKET_REQUEST = 0,
	NAN_EHT_PACKET_RESPONSE = 1,
	NAN_EHT_PACKET_CONFIRM = 2,
};

enum NAN_EHT_MODE {
	NAN_EHT_MODE_LEGACY = 0,
	NAN_EHT_MODE_1 = 1,
	NAN_EHT_MODE_2 = 2,
	NAN_EHT_MODE_3 = 3,
	NAN_EHT_MODE_4 = 4,
	NAN_EHT_MODE_DEFAULT = 2,
};

enum NAN_EHT_REASON {
	/* Type == 0b01 (Response) */
	NAN_EHT_REASON_RESPONSE_SUCCESS = 0,
	NAN_EHT_REASON_RESPONSE_VSIE = 1,
	NAN_EHT_REASON_RESPONSE_OPMODE = 2,
	NAN_EHT_REASON_RESPONSE_NOT_SUPPORTED = 3,
	/* Type == 0b10 (Confirm) */
	NAN_EHT_REASON_CONFIRM_SUCCESS = 0,
	NAN_EHT_REASON_CONFIRM_NDP = 1,
	NAN_EHT_REASON_CONFIRM_OPMODE = 2,
	NAN_EHT_REASON_CONFIRM_FAIL = 3,
	NAN_EHT_REASON_CONFIRM_PEER_NO_EHT = 4,
	NAN_EHT_REASON_CONFIRM_FAIL_DUT = 5,
};

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
struct IE_NAN_EHT_PENDING_CMD {
	struct LINK_ENTRY rLinkEntry;
	uint8_t ucNanOui;
	u8 cmd[NAN_MAX_EXT_DATA_SIZE];
};

/* [v2.1] Table 38. EHT config command */
struct IE_NAN_EHT_CMD {
	/* 0x3D = 61 */
	uint8_t ucNanOui; /* NAN_EHT_CMD_OUI */
	/* 0x0000 ~ 0xFFFF */
	uint16_t u2Length;
	/* 0x0 ~ 0xF */
	uint8_t ucMajorVersion;
	/* 0x0 ~ 0xF */
	uint8_t ucMinorVersion;
	/* 0x00 ~ 0xFF */
	uint8_t ucRequestId;
	/*
	 * Type:
	 * NAN EHT Configuration
	 * packet type
	 *
	 * 00: Request
	 * 01: Response
	 * 10: Confirm
	 */
	/*
	 * Mode
	 * NAN EHT operation mode
	 *
	 * 0b000: Legacy NAN 6E
	 * 0b001: Mode 1
	 * 0b010: Mode 2
	 * 0b011: Mode 3
	 * 0b100: Mode 4
	 *
	 */
	uint8_t type:2,
		mode:3,
		reserved_1:3;
	uint32_t reserved_2;
} __KAL_ATTRIB_PACKED__;

#define EHT_CMD_HDR_LEN OFFSET_OF(struct IE_NAN_EHT_CMD, ucMajorVersion)
#define EHT_CMD_LEN(fp) (((struct IE_NAN_EHT_CMD *)(fp))->u2Length)
#define EHT_CMD_SIZE(fp) (EHT_CMD_HDR_LEN + EHT_CMD_LEN(fp))
#define EHT_CMD_BODY_SIZE(fp)	\
	(EHT_CMD_SIZE(fp) - sizeof(struct IE_NAN_EHT_CMD))

/* [v2.1] Table 39. EHT config event */
struct IE_NAN_EHT_EVENT {
	/* 0x3D = 61 */
	uint8_t ucNanOui; /* NAN_EHT_EVT_OUI */
	/* 0x0000 ~ 0xFFFF */
	uint16_t u2Length;
	/* 0x0 ~ 0xF */
	uint8_t ucMajorVersion;
	/* 0x0 ~ 0xF */
	uint8_t ucMinorVersion;
	/* 0x00 ~ 0xFF */
	uint8_t ucRequestId;
	/*
	 * Type:
	 * NAN EHT Configuration
	 * packet type
	 *
	 * 00: Request
	 * 01: Response
	 * 10: Confirm
	 */
	/*
	 * For Type 01 (Response)
	 * 000: Successful (General)
	 * 001: NAN EHT mode capable
	 *      and VSIE sent as a default
	 * 010: Successfully OP mode
	 *      indication changed in VSIE
	 *      according to private
	 *      command
	 * 011: Not supported
	 *      configuration
	 * 100~111: Reserved
	 *
	 * For Type 10 (Confirm)
	 * 000: Successful (General)
	 * 001: Successfully NAN EHT NDP
	 *      setup
	 * 010: Successfully NAN EHT OP
	 *      mode changed
	 * 011: Failed (General)
	 * 100: Failed (Peer is not
	 *      supporting NAN EHT –
	 *      No NAN EHT VSIE in peer’s
	 *      SDF nor NDP frame)
	 * 101: Failed (DUT issue)
	 */
	/*
	 * Mode
	 * NAN EHT operation mode
	 *
	 * 0b000: Legacy NAN 6E
	 * 0b001: Mode 1
	 * 0b010: Mode 2
	 * 0b011: Mode 3
	 * 0b100: Mode 4
	 *
	 */
	uint8_t type:2,
		reason:3,
		mode:3;
	uint32_t reserved_2;

} __KAL_ATTRIB_PACKED__;

#define EHT_EVENT_HDR_LEN OFFSET_OF(struct IE_NAN_EHT_EVENT, ucMajorVersion)
#define EHT_EVENT_LEN(fp) (((struct IE_NAN_EHT_EVENT *)(fp))->u2Length)
#define EHT_EVENT_SIZE(fp) (EHT_EVENT_HDR_LEN + EHT_EVENT_LEN(fp))
#define EHT_EVENT_BODY_SIZE(fp)	\
	(EHT_EVENT_SIZE(fp) - sizeof(struct IE_NAN_EHT_EVENT))

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

uint32_t nanProcessEHT(struct ADAPTER *prAdapter, const uint8_t *buf,
		       size_t u4Size);
void nanEhtNanOffHandler(struct ADAPTER *prAdapter);
void nanEhtNanOnHandler(struct ADAPTER *prAdapter);
void nanSet6gConfig(struct ADAPTER *ad);

#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_EXT_EHT_H_ */

