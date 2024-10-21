/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_ASC_H_
#define _NAN_EXT_ASC_H_

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
enum NAN_ASC_SUBTYPE {
    NAN_ASC_SUBTYPE_RETURN_TO_ORIGINAL_NAN = 0,
    NAN_ASC_SUBTYPE_ANOTHER_NAN_CLUSTER,
    NAN_ASC_SUBTYPE_CONNECTED_AP,
    NAN_ASC_SUBTYPE_OTHER_PROXY,
    NAN_ASC_SUBTYPE_MAINTAIN_CUR_NAN_TAKE_DIFF_DW_OFF,
    /* The following is MTK defined */
	NAN_ASC_SUBTYPE_SYNC_BCN_REPORT,
	NAN_ASC_SUBTYPE_DISC_BCN_REPORT,
    NAN_ASC_SUBTYPE_NUM
};

enum NAN_ASC_PACKET_TYPE {
	NAN_ASC_PACKET_REQUEST = 0,
	NAN_ASC_PACKET_RESPONSE = 1,
};

enum NAN_ASC_STATUS {
	NAN_ASC_STATUS_SUCCESS = 0,
	NAN_ASC_STATUS_SUCCESS_BUT_CHANGED = 1,
	NAN_ASC_STATUS_FAIL = 2,
	NAN_ASC_STATUS_ASYNC_EVENT = 3,
};

struct NAN_ASC_PENDING_CMD {
	uint8_t ucOui;
	uint8_t ucRequestId;
	uint8_t type;
	uint8_t subtype;
	uint8_t power_save;
	uint8_t ucTrackEnable;
	uint8_t ucOpChannel;
	uint8_t ucOpBand;
	uint8_t aucBSSID[MAC_ADDR_LEN];
	uint8_t ucIsAscMode;
	union ULARGE_INTEGER u8Tsf;
	uint16_t tsf_option;
	void *cmd; /* IE_NAN_ASC_CMD, pinter of requested command from HAL */
};

struct IE_NAN_ASC_PENDING_CMD {
	struct LINK_ENTRY rLinkEntry;
	void *cmd; /* IE_NAN_ASC_CMD, pinter of requested command from HAL */
};

/* [v1.8] Table 1. ASC data */
struct IE_NAN_ASC_CMD {
	/* 0x39 = 57 */
	uint8_t ucNanOui; /* NAN_ASC_EVT_OUI */
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
	 * 00: Request <--
	 * 01: Response
	 * 10: Confirm
	 *
	 * BITS(3, 5): Subtype
	 * When ucType = 00
	 * 000: Return to original NAN
	 * 001: Another NAN Cluster
	 * 010: Connected AP
	 * 011: Other proxy
	 * 100: Maintain current NAN and
	 *		take a different DW offset
	 * When ucType = 01
	 * 100: ASC Failed
	 * 101: Cluster joined
	 * 110: Cluster initiated
	 *
	 * BIT(2): Power save
	 * 0: No, maintain current NAN role
	 * 1: Enter, master becomes NMNS
	 *
	 * BIT(1): Track enable
	 * 0: Disable or Do not care
	 * 1: Enable
	 */
	uint8_t type:2,
		subtype:3,
		power_save:1,
		track_en:1,
		reserved:1;
	/*
	 * Operating channel
	 */
	uint8_t ucOpChannel;
	/*
	 * BITS(5, 7): Operating band
	 * 000: Sub 1-GHz
	 * 001: 2.4 GHz
	 * 010: 5 GHz
	 * 011: 6 GHz
	 * 100: 60 GHz
	 */
	uint8_t ucOpBand:3,
		reserved2:5;
	/*
	 * 00:00:00:00:00:01 ~
	 * FF:FF:FF:FF:FF:FF
	 */
	uint8_t aucBSSID[MAC_ADDR_LEN];
	/* 0 ~ FFFFFFFFFFFFFFFE */
	union ULARGE_INTEGER u8Tsf;
	/*
	 * BIT(15), TSF option
	 * 0: TSF,
	 *	  new TSF = TSF field
	 * 1: TSF offset,
	 *	  new TSF = current TSF - TSF field
	 * BITS(0, 13)
	 * Bcn interval
	 */
	uint16_t tsf_option:1,
		 reserved3:15;
	/*
	 * TBD?
	 * Removed in ver1.8
	 * uint16_t u2ServiceData;
	 */
} __KAL_ATTRIB_PACKED__;

#define ASC_CMD_HDR_LEN OFFSET_OF(struct IE_NAN_ASC_CMD, ucMajorVersion)
#define ASC_CMD_LEN(fp) (((struct IE_NAN_ASC_CMD *)(fp))->u2Length)
#define ASC_CMD_SIZE(fp) (ASC_CMD_HDR_LEN + ASC_CMD_LEN(fp))
#define ASC_CMD_BODY_SIZE(fp)	\
	(ASC_CMD_SIZE(fp) - sizeof(struct IE_NAN_ASC_CMD))

/* [v1.8] Table 2. ASC event data format */
/* Action Byte, ucAction */
#define IE_NAN_ASC_EVENT_TYPE_MASK          BITS(6, 7)
#define IE_NAN_ASC_EVENT_TYPE_SHFT          6
#define IE_NAN_ASC_EVENT_STATUS_MASK          BITS(4, 5)
#define IE_NAN_ASC_EVENT_STATUS_SHFT          4
#define IE_NAN_ASC_EVENT_REASON_MASK          BITS(0, 3)
#define IE_NAN_ASC_EVENT_REASON_SHFT          0

#define SET_NAN_ASC_EVENT_TYPE(_s, _val) \
{\
	SET_NAN_FIELD(_s, \
		_val, \
		ucAction, \
		IE_NAN_ASC_EVENT_TYPE_MASK, \
		IE_NAN_ASC_EVENT_TYPE_SHFT) \
}
#define GET_NAN_ASC_EVENT_TYPE(_s) \
	GET_NAN_FIELD(_s, \
		ucAction, \
		IE_NAN_ASC_EVENT_TYPE_MASK, \
		IE_NAN_ASC_EVENT_TYPE_SHFT)
#define SET_NAN_ASC_EVENT_STATUS(_s, _val) \
{\
	SET_NAN_FIELD(_s, \
		_val, \
		ucAction, \
		IE_NAN_ASC_EVENT_STATUS_MASK, \
		IE_NAN_ASC_EVENT_STATUS_SHFT) \
}
#define GET_NAN_ASC_EVENT_STATUS(_s) \
	GET_NAN_FIELD(_s, \
		ucAction, \
		IE_NAN_ASC_EVENT_STATUS_MASK, \
		IE_NAN_ASC_EVENT_STATUS_SHFT)
#define SET_NAN_ASC_EVENT_REASON(_s, _val) \
{\
	SET_NAN_FIELD(_s, \
		_val, \
		ucAction, \
		IE_NAN_ASC_EVENT_REASON_MASK, \
		IE_NAN_ASC_EVENT_REASON_SHFT) \
}
#define GET_NAN_ASC_EVENT_REASON(_s) \
	GET_NAN_FIELD(_s, \
		ucAction, \
		IE_NAN_ASC_EVENT_REASON_MASK, \
		IE_NAN_ASC_EVENT_REASON_SHFT)

struct IE_NAN_ASC_EVENT {
	/* 0x39 = 57 */
	uint8_t ucNanOui; /* NAN_ASC_EVT_OUI */
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
	 * 01: Response <--
	 * 10: Confirm
	 *
	 * BITS(4, 5): Status
	 * When ucType = 00
	 * 00: Success
	 * 01: Success but changed
	 * 10: Fail
	 * 11: Async event
	 *
	 * BITS(0, 3): Reason code
	 * For Status 00~10
	 *
	 * 0d0: Success
	 * 0d1: AP not found
	 * 0d2: Proxy not found
	 * 0d3: Sync to AP TSF fail
	 * 0d4: Sync to Proxy TSF fail
	 * 0d5: TSF manual change fail
	 * 0d6: invalid TSF
	 * 0d7: invalid TSF offset
	 * 0d8: NAN Cluster not found
	 * 0d9: Switching channel failed
	 * 0d10~0d13: Reserved
	 * 0d14: Command parsing error
	 * 0d15: Undefined failure
	 *
	 * For Status 11 (Asynchronous event)
	 * 0d0: NAN cluster initiated
	 * 0d1: Joined to a new NAN cluster
	 * 0d2: New NAN device joined
	 * 0d3: AP-NAN PS terminated due to beacon loss,
	 *      and switched to AScC Power save
	 * 0d4: AP-NAN PS terminated
	 *      with other reasons
	 * 0d5: AP-NAN PS terminated
	 *      with legacy NAN function call
	 */
	uint8_t type:2,
		status:2,
		reason_code:4;

	/*
	 * struct NAN_ASC_EVENT_CLUSTER
	 */
	uint8_t aucInfo[];
} __KAL_ATTRIB_PACKED__;

#define ASC_EVENT_HDR_LEN OFFSET_OF(struct IE_NAN_ASC_EVENT, ucMajorVersion)
#define ASC_EVENT_LEN(fp) (((struct IE_NAN_ASC_EVENT *)(fp))->u2Length)
#define ASC_EVENT_SIZE(fp) (ASC_EVENT_HDR_LEN + ASC_EVENT_LEN(fp))
#define ASC_EVENT_BODY_SIZE(fp)	\
	(ASC_EVENT_SIZE(fp) - sizeof(struct IE_NAN_ASC_EVENT))

/* [v1.8] Table 3. Asynchronous event field for ASC */
/* NAN cluster initiated, reason code == 0 */
struct NAN_ASC_EVENT_CLUSTER_INIT {
	uint8_t aucClusterId[MAC_ADDR_LEN];
	uint8_t aucAnchorMasterRank[ANCHOR_MASTER_RANK_NUM];
	uint8_t aucOwnNMI[MAC_ADDR_LEN];
} __KAL_ATTRIB_PACKED__;

/* Joined to a new NAN cluster, reason code == 1 */
struct NAN_ASC_EVENT_CLUSTER_JOIN {
	uint8_t aucClusterId[MAC_ADDR_LEN];
	uint8_t aucAnchorMasterRank[ANCHOR_MASTER_RANK_NUM];
	uint8_t aucMasterNMI[MAC_ADDR_LEN];
	uint8_t aucOwnNMI[MAC_ADDR_LEN];
} __KAL_ATTRIB_PACKED__;

/* New NAN Device joined, reason code == 2 */
struct NAN_ASC_EVENT_CLUSTER_NEW_DEVICE {
	uint8_t aucNewDeviceNMI[MAC_ADDR_LEN];
	uint8_t rssi;
} __KAL_ATTRIB_PACKED__;

/* Sync beacon track, reason code == 7 */
struct NAN_ASC_EVENT_SYNC_BEACON {
	uint8_t aucClusterId[MAC_ADDR_LEN];
	uint8_t aucAnchorMasterRank[ANCHOR_MASTER_RANK_NUM];
	uint8_t aucMasterNMI[MAC_ADDR_LEN];
	uint64_t u8TsfOffset;
} __KAL_ATTRIB_PACKED__;

/* Disc beacon track, reason code == 8 */
struct NAN_ASC_EVENT_DISC_BEACON {
	uint8_t aucClusterId[MAC_ADDR_LEN];
	uint8_t aucMasterNMI[MAC_ADDR_LEN];
	uint64_t u8TsfOffset;
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

uint32_t nanProcessASC(struct ADAPTER *prAdapter, const uint8_t *buf,
		       size_t u4Size);
void nanExtSetAsc(struct ADAPTER *prAdapter);

#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_EXT_ASC_H_ */

