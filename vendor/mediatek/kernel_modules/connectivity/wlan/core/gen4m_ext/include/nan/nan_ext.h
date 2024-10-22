/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_H_
#define _NAN_EXT_H_

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#if CFG_SUPPORT_NAN
/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

extern uint8_t g_ucNanIsOn;

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* ucNanOui [V2.1] 3.2.5 */
enum EXT_CMD_EVENT_OUI {
	NAN_CCM_CMD_OUI = 0x34, /* 52, TBD */
	NAN_EXT_CMD_BASE = NAN_CCM_CMD_OUI,
	NAN_CCM_EVT_OUI = 0x35, /* 53, TBD */
	/* empty 0x36 */
	NAN_PA_CMD_OUI = 0x37, /* 55, TBD */
	NAN_MDC_CMD_OUI = 0x38, /* 56, TBD */
	NAN_ASC_CMD_OUI = 0x39, /* 57, Table 1, ANC */
	NAN_ASC_EVT_OUI = NAN_ASC_CMD_OUI, /* 57, 3.3.1.3, Table 2 */
	NAN_AMC_CMD_OUI = 0x3A, /* 58, Table 4 */
	NAN_ASCC_CMD_OUI = 0x3B, /* 59, Table 5, ANS, ANC */
	NAN_ASCC_EVT_OUI = NAN_ASCC_CMD_OUI, /* 59, 3.3.3.7, Table 14 */
	NAN_FR_EVT_OUI = 0x3C, /* 60, Table 32 */
	NAN_EHT_CMD_OUI = 0x3D, /* 61, Table 38 */
	NAN_EHT_EVT_OUI = NAN_EHT_CMD_OUI, /* 61, 3.3.15.7, Table 39 */
	NAN_ASON_CMD_OUI = 0x3E, /* 62, Assitive Scan over NAN */
	NAN_ADSDC_TSF_OUI = 0x3F, /* 63, Table 18 */
	NAN_ADSDC_CMD_OUI = 0x40, /* 64, Table 17, ANC */
	NAN_EXT_CMD_END = NAN_ADSDC_CMD_OUI,
};

/* VSIE */
#define NAN_EHT_VSIE_OUI (0x41) /* 65, Table 34 */

#ifndef __KAL_ATTRIB_ALIGNED_FRONT__
#define __KAL_ATTRIB_ALIGNED_FRONT__(x)
#endif
#ifndef __KAL_ATTRIB_ALIGNED__
#define __KAL_ATTRIB_ALIGNED__(x)
#endif

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
#ifndef BIT
#define BIT(n)                          ((uint32_t) 1UL << (n))
#endif /* BIT */
#ifndef BITS
#define BITS(m, n)                       (~(BIT(m)-1) & ((BIT(n) - 1) | BIT(n)))
#endif /* BIT */

#define SET_NAN_FIELD(_s, _val, _field, _mask, _shft) \
{ \
	_s->_field &= ~(_mask); \
	_s->_field |= \
		((_val << _shft) \
			& _mask); \
}
#define GET_NAN_FIELD(_s, _field, _mask, _shft) \
	((_s->_field & _mask) \
	>> _shft)

struct NAN_ASC_ASYNC_EVENT {
	uint8_t ucEventType;
	uint8_t ucClusterId[MAC_ADDR_LEN];
	uint8_t aucAnchorMasterRank[ANCHOR_MASTER_RANK_NUM];
	uint8_t ucOwnNmi[MAC_ADDR_LEN];
	uint8_t ucMasterNmi[MAC_ADDR_LEN];
	uint64_t u8TsfOffset;
};

enum NAN_ASC_EVENT {
	/* Type == 0b00~10  */
	NAN_ASC_EVENT_SUCCESS = 0,
	NAN_ASC_EVENT_AP_NOT_FOUND = 1,
	NAN_ASC_EVENT_PROXY_NOT_FOUND = 2,
	NAN_ASC_EVENT_SYNC_AP_TSF_FAIL = 3,
	NAN_ASC_EVENT_SYNC_PROXY_TSF_FAIL = 4,
	NAN_ASC_EVENT_TSF_CHANGE_FAIL = 5,
	NAN_ASC_EVENT_TSF_INVALID = 6,
	NAN_ASC_EVENT_TSF_INVALID_OFFSET = 7,
	NAN_ASC_EVENT_CLUSTER_NOT_FOUND = 8,
	NAN_ASC_EVENT_SWITCH_CH_FAIL = 9,
	NAN_ASC_EVENT_FAIL_NOT_SUPPORTED = 13,
	NAN_ASC_EVENT_FAIL_PARSING_ERROR = 14,
	NAN_ASC_EVENT_FAIL_UNDEFINED = 15,
	/* Type == 0b11 (Async event) */
	NAN_ASC_EVENT_INIT_CLUSTER = 0,
	NAN_ASC_EVENT_JOIN_CLUSTER = 1,
	NAN_ASC_EVENT_NEW_DEVICE_JOIN = 2,
	NAN_ASC_EVENT_ASCC_END_PS = 3,
	NAN_ASC_EVENT_ASCC_END_OTHERS = 4,
	NAN_ASC_EVENT_ASCC_END_LEGACY = 5,
	NAN_ASC_EVENT_NEW_CLUSTER_FOUND = 6,
	NAN_ASC_EVENT_SYNC_BEACON_TRACK = 7,
	NAN_ASC_EVENT_DISC_BEACON_TRACK = 8,
};

/**
 * Common header of various command types,
 * dispatch to corresponding handler according to the OUI field
 * Same:
 *	Table 1 ASC data (OUI=57)
 *	Table 2 ASC event (OUI=57)
 *	Table 4 AMC data (OUI=58)
 *	Table 5 AScC data (OUI=59)
 *	Table 13 AScC event (OUI=59)
 *	Table 16 ADSDC data (OUI=64)
 *	Table 17 ADSDC event (OUI=64)
 *	Table 21 CCM data (OUI=52)
 *	Table 36 Fast recovery data (OUI=60)
 *	Table 42 NAN EHT command (OUI=61)
 *	Table 43 NAN EHT event (OUI=61)
 * TODO: Different
 *	Table 22 CCM merging indication data (OUI=53)
 *	Table 29 MDC data (OUI=56)
 *	Table 31 PA-NAN (OUI=55)
 */
struct IE_NAN_EXT_COMMON_HEADER {
	uint8_t ucOui;
	uint16_t u2Length;
	uint8_t ucMajorVersion;
	uint8_t ucMinorVersion;
	uint8_t ucRequestId;
} __KAL_ATTRIB_PACKED__;

#define EXT_MSG_HDR_LEN \
	OFFSET_OF(struct IE_NAN_EXT_COMMON_HEADER, ucMajorVersion)
#define EXT_MSG_LEN(fp) (((struct IE_NAN_EXT_COMMON_HEADER *)(fp))->u2Length)
#define EXT_MSG_SIZE(fp) (EXT_MSG_HDR_LEN + EXT_MSG_LEN(fp))

/* Table 38, NAN EHT VSIE */
/* Table 41, NAN EHT Operation mode */
/* Table 42, NAN EHT Configuration */
/* Table 43, NAN EHT Event */

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

/* OUI(u8) + Length(u16) */
#define NAN_EXT_CMD_HDR_LEN \
		OFFSET_OF(struct IE_NAN_EXT_COMMON_HEADER, ucMajorVersion)
#define NAN_EXT_EVENT_HDR_LEN	\
		OFFSET_OF(struct IE_NAN_EXT_COMMON_HEADER, ucMajorVersion)


/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

const char *oui_str(uint8_t i);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#if 0
int32_t nanInitFs(void);
int32_t nanUninitFs(void);
#endif

#if CFG_SUPPORT_NAN_EXT
uint32_t nanExtParseCmd(struct ADAPTER *prAdapter,
			uint8_t *buf, uint16_t *u2Size);
uint32_t nanExtSendIndication(struct ADAPTER *prAdapter,
			      void *buf, uint16_t u2Size);
uint32_t nanIndicateFrDeleted(struct ADAPTER *prAdapter,
			  struct _NAN_NDL_INSTANCE_T *prNDL);
void iniFileErrorCheck (struct ADAPTER *prAdapter, uint8_t **ppucIniBuf,
	uint32_t *pu4ReadSize);
void nanExtEnableReq(struct ADAPTER *prAdapter);
void nanExtDisableReq(struct ADAPTER *prAdapter);
void nanExtComposeClusterEvent(struct ADAPTER *prAdapter, struct NAN_DE_EVENT *prDeEvt);
void nanExtComposeBeaconTrack(struct ADAPTER *prAdapter,
				struct _NAN_EVENT_REPORT_BEACON *prFwEvt);
void nanExtSendAsyncEvent(struct ADAPTER *prAdapter,
				struct NAN_ASC_ASYNC_EVENT *prAscAsyncEvt);
void nanExtTerminateApNan(struct ADAPTER *prAdapter, uint8_t ucReason);
void nanAdsdcBackToNormal(struct ADAPTER *prAdapter);
void nanExtClearCustomNdpFaw(uint8_t ucIndex);
void nanPeerReportEhtEvent(struct ADAPTER *prAdapter,
	uint8_t enable);
void nanEnableEhtMode(struct ADAPTER *ad,
	uint8_t mode);
void nanEnableEht(struct ADAPTER *ad,
	uint8_t enable);
#else /* dummy functions called from gen4m to ext */
static inline void nanIndicateFrDeleted(struct ADAPTER *prAdapter,
					struct _NAN_NDL_INSTANCE_T *prNDL)
{
	return WLAN_STATUS_SUCCESS;
}

static inline void nanExtEnableReq(struct ADAPTER *prAdapter)
{
}
void nanExtDisableReq(struct ADAPTER *prAdapter)
{
}
static inline void nanExtClearCustomNdpFaw(uint8_t ucIndex) { }
static inline void nanPeerReportEhtEvent(
	struct ADAPTER *prAdapter, uint8_t enable)
{
}
static inline void nanEnableEhtMode(
	struct ADAPTER *prAdapter, uint8_t mode)
{
}
static inline void nanEnableEht(
	struct ADAPTER *prAdapter, uint8_t enable)
{
}
#endif

#endif
#endif /* _NAN_EXT_H_ */
