/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_PA_H_
#define _NAN_EXT_PA_H_

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

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/* [v1.8] Table 31. P2P or NAN VSIE for PA-NAN Configuration (OUI 55) */
struct IE_NAN_PA_CMD {
	/* 0x37 = 55 */
	uint8_t ucNanOui; /* NAN_PA_CMD_OUI */
	/* 0d33, TBD? */
	uint8_t ucLength; /* TODO: need to align other structure as uint16_t? */
	/* 0x0 ~ 0xF */
	uint8_t ucMajorVersion; /* TODO: defined as 4-bit in v1.8 */
	/* 0x0 ~ 0xF */
	uint8_t ucMinorVersion; /* TODO: defined as 4-bit in v1.8 */
	/* 0x00 ~ 0xFF */
	uint8_t ucRequestId;
	/*
	 * BITS(0, 3): Type subfield as follows
	 * 00: Request
	 * 01: Response
	 * 10: Confirm
	 * 11: Initiation
	 * BITS(4, 7): Status subfield
	 * 00: Accepted
	 * 01: Rejected
	 * 10: Continued
	 * 11: Cancel
	 */
	/* TODO: field define is inconsistency in v1.8 */
	uint8_t type:4,
		status:4;

	/*
	 * Minor reason code
	 * TBD?
	 * TODO: seems to be a 8-bit field, but noted as 1-bit in v1.8
	 */
	uint8_t ucMinorReason;

	/*
	 * PA-NAN Capability
	 * TBD?
	 *
	 * Service request bit
	 * TBD?
	 *
	 * NDP Setup request bit
	 * TBD?
	 */
	uint8_t capability:1,
		service_request:1,
		ndp_setup_request:1,
		reserved:5;

	/**
	 * Followed by various structures, determine by the common fields in
	 * at the starting of the header, and casting to defined structure
	 * accordingly.
	 * 1. PA-NAN information (Table 33)
	 *    Related NAN attribute for Synchronization, Master, and Cluster
	 * 2. PA-NAN service information (Table 34)
	 *    Related NAN attribute for Service Discovery
	 * 3. PA-NAN NDP setup information (Table 35)
	 *    Related NAN attribute for NDP setup, NAN Pairing
	 */
	uint8_t aucData[];
} __KAL_ATTRIB_PACKED__;

#define PA_CMD_HDR_LEN OFFSET_OF(struct IE_NAN_PA_CMD, ucMajorVersion)
#define PA_CMD_LEN(fp) (((struct IE_NAN_PA_CMD *)(fp))->u2Length)
#define PA_CMD_SIZE(fp) (PA_CMD_HDR_LEN + PA_CMD_LEN(fp))
#define PA_CMD_BODY_SIZE(fp)	\
	(PA_CMD_SIZE(fp) - sizeof(struct IE_NAN_PA_CMD))

/* [v1.8] Table 32. PA-NAN Information field and description */
__KAL_ATTRIB_PACKED_FRONT__
struct _NAN_ATTR_VENDOR {
	uint8_t ucAttrId;
	uint8_t ucLength;
	uint8_t aucSenderNMI[MAC_ADDR_LEN];
	uint8_t aucReceiverNMI[MAC_ADDR_LEN];
} __KAL_ATTRIB_PACKED__;

__KAL_ATTRIB_PACKED_FRONT__
struct IE_NAN_PA_INFO {
	struct _NAN_ATTR_MASTER_INDICATION_T rMasterIndAttr;
	struct _NAN_ATTR_CLUSTER_T rClusterAttr;
	struct _NAN_ATTR_DEVICE_CAPABILITY_T rDevAttr;
	struct _NAN_ATTR_VENDOR rVendorAttr;
	/* ASC, TBD? */
} __KAL_ATTRIB_PACKED__;

/* [v1.8] Table 34. PA-NAN Service information field and description */
__KAL_ATTRIB_PACKED_FRONT__
struct IE_NAN_PA_SERVICE_INFO {
	struct _NAN_ATTR_SDA_T rSdaAttr;
	struct _NAN_ATTR_SDEA_T rSdeaAttr;
	struct _NAN_ATTR_NAN_AVAILABILITY_T rAvailAttr;
	/* NAN_ATTR_ID_NAN_IDENTITY_RESOLUTION, TBD? */
	/* NAN_ATTR_ID_NAN_PAIRING_BOOTSTRAPPING, TBD? */
} __KAL_ATTRIB_PACKED__;

/* [v1.8] Table 35. PA-NAN NDP setup information field and description */
__KAL_ATTRIB_PACKED_FRONT__
struct IE_NAN_PA_NDP_INFO {
	struct _NAN_ATTR_NDPE_T *rNDPEAttr;
	struct _NAN_ATTR_NAN_AVAILABILITY_T rAvailAttr;
	struct _NAN_ATTR_ELEMENT_CONTAINER_T rECAttr;
	struct _NAN_ATTR_CIPHER_SUITE_INFO_T rCipherAttr;
	struct _NAN_ATTR_SECURITY_CONTEXT_INFO_T rSecAttr;
	struct _NAN_ATTR_SHARED_KEY_DESCRIPTOR_T rSKAttr;
} __KAL_ATTRIB_PACKED__;


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

uint32_t nanProcessPA(struct ADAPTER *prAdapter, const uint8_t *buf,
		      size_t u4Size);

#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_EXT_PA_H_ */

