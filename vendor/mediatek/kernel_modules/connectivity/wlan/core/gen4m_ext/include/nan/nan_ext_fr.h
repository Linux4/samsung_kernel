/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_FR_H_
#define _NAN_EXT_FR_H_

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

enum NAN_FR_PACKET_TYPE {
	NAN_FR_PACKET_REQUEST = 0,
	NAN_FR_PACKET_RESPONSE = 1,
	/* 2,3: RESERVED */
};

enum NAN_FR_PACKET_SUBTYPE {
	NAN_FR_REQUEST_REMOVE_DEVICES = 0,
	NAN_FR_REQUEST_REMOVE_ALL = 1,
	NAN_FR_REQUEST_CHECK_DEVICES = 2,
	NAN_FR_REQUEST_RESERVED = 3,

	NAN_FR_RESPONSE_SUCCESS = 0,
	NAN_FR_RESPONSE_FAIL = 1,
	NAN_FR_RESPONSE_ASYNC = 2,
	NAN_FR_RESPONSE_RESERVED = 3,
};

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/**
 * [v2.1] Table 33. Fast recovery device entry
 * @device_id: used for indicating one peer Fast Recovery device
 * @aucPeerNMIAddr: 0x000000000000~0xFFFFFFFFFFFF: Peer NMI
 */
struct IE_NAN_FR_DEVICE_ENTRY {
	uint8_t device_id:4,
		reserved:4;

	uint8_t aucPeerNMIAddr[MAC_ADDR_LEN];
} __KAL_ATTRIB_PACKED__;

/**
 * [v1.8] Table 32. Fast recovery data
 * @ucNanOui: Vendor specific OUI type, 0x3B = 59
 *
 * @u2Length: Length of this OUI data, counted after this field
 *
 * @ucMajorVersion: Version of STD+
 *
 * @ucMinorVersion: Version of STD+
 *
 * @ucRequestId: ID for distinguishing requested data
 *
 * @type: FR packet type
 *   Ref: enum NAN_FR_PACKET_TYPE
 *
 * @subtype: FR packet subtype
 *   Ref: enum NAN_FR_PACKET_SUBTYPE
 *
 * @ucNumDev: Number of fast recovery devices
 *
 * @aucDeviceEntries: currently active PS devices with this device
 *
 * Context:
 * FW or host shall generate corresponding Fast Recovery event when
 * 1) there is framework request in regard to Fast Recovery Status check
 *    TODO: ASCC.usage == current schedule check?
 *
 * 2) the new Fast Recovery device is successfully registered
 *    ASCC.usage == NAN_SCHEDULE_USAGE_ASCC_PS_FR or
 *    ASCC.usage == NAN_SCHEDULE_USAGE_APNAN_PS_FR
 *    TODO: when to trigger confirm after AScC command processed?
 *
 * 3) STD+ DUT wants to remove the specific device
 *    nanIndicateFrDeleted
 *
 * When framework wants to try this function, it can set type as request for
 * each.
 */
struct IE_NAN_FR_EVENT {
	uint8_t ucNanOui; /* NAN_FR_EVT_OUI */
	uint16_t u2Length;
	uint8_t ucMajorVersion;
	uint8_t ucMinorVersion;
	uint8_t ucRequestId;

	uint8_t type:2,
		subtype:2,
		reserved:4;

	uint8_t ucNumDev;

	struct IE_NAN_FR_DEVICE_ENTRY arDevice[];
} __KAL_ATTRIB_PACKED__;

#define FR_EVENT_HDR_LEN OFFSET_OF(struct IE_NAN_FR_EVENT, ucMajorVersion)
#define FR_EVENT_LEN(fp) (((struct IE_NAN_FR_EVENT *)(fp))->u2Length)
#define FR_EVENT_SIZE(fp) (FR_EVENT_HDR_LEN + FR_EVENT_LEN(fp))
#define FR_EVENT_BODY_SIZE(fp)	\
	(FR_EVENT_SIZE(fp) - sizeof(struct IE_NAN_FR_EVENT))


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

uint32_t nanProcessFR(struct ADAPTER *prAdapter, const uint8_t *buf,
		      size_t u4Size);

#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_EXT_FR_H_ */

