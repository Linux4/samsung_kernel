/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_AMC_H_
#define _NAN_EXT_AMC_H_

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

/* [v2.1] Table 4. AMC data */
struct IE_NAN_AMC_CMD {
	/* 0x3A = 58 */
	uint8_t ucNanOui; /* NAN_AMC_CMD_OUI */
	/* 0x0000 ~ 0xFFFF */
	uint16_t u2Length;
	/* 0x00 ~ 0xFF */
	uint8_t ucMajorVersion;
	/* 0x00 ~ 0xFF */
	uint8_t ucMinorVersion;
	/* 0x00 ~ 0xFE */
	uint8_t ucRequestId;
	/*
	 * BITS(0, 1): Type
	 * 00: Request
	 * 01: Response
	 * 10: Confirm
	 * 11: Reserved
	 *
	 * BIT(2): Mergeing start
	 * 0: No
	 * 1: Merge
	 *
	 * BITS(3, 5): Specify merging timing
	 * 000: on-demand (instant)
	 * 001: Next DW
	 * 010: Next DW 0
	 * 011: Specified DW
	 * 100~111: Reserved
	 *
	 * BIT(6): MP control support
	 * 0: Not support
	 * 1: Support
	 *
	 * BIT(7): Merging feature enabled
	 * 0: NAN Merging disabled
	 * 1: NAN Merging enabled
	 */
	uint8_t cmd_type:2,
		merge_start:1,
		merge_timing:3,
		mp_ctrl_support:1,
		merging_feature_enabled:1;
	 /*
	 * BIT(0): Merging Indication Broadcast
	 * 0: NMNS not send merging indicating frame
	 * 1: NMNS send merging indicating frame once
	 *
	 * BITS(1, 3): Merging Indication Interval
	 * 0: every 64TU, transmit merging indication
	 * 0d1~0d7: every 64*(N+1) TUs, transmit merging indication
	 *
	 * BIT(4): MDC enabled
	 * 0: Standard NAN merging
	 * 1: Conditional NAN Merging (MDC) enabled
	 *
	 * BITS(5, 7): Reserved
	 */
	uint8_t merging_indication_brodcast:1,
		merging_indication_interval:3,
		mdc_enabled:1,
		reserved:3;
	/*
	 * 0: Do not update MP
	 * 1~180: Wanted MP value
	 * 181~255: Reserved
	 */
	uint8_t ucMasterPref;
	uint8_t ucReserved;
	/*
	 * 1st bit: MDC
	 * 2nd bit: CCM
	 * 3rd~8th bit: Reserved
	 */
	struct {
		uint8_t mdc:1,
				ccm:1,
				reserved:6;
	} additional_info;
	/*
	 * MDC data (Optional)
	 * OUI 56
	 * Length: 8 bytes
	 */
} __KAL_ATTRIB_PACKED__;

#define AMC_CMD_HDR_LEN OFFSET_OF(struct IE_NAN_AMC_CMD, ucMajorVersion)
#define AMC_CMD_LEN(fp) (((struct IE_NAN_AMC_CMD *)(fp))->u2Length)
#define AMC_CMD_SIZE(fp) (AMC_CMD_HDR_LEN + AMC_CMD_LEN(fp))
#define AMC_CMD_BODY_SIZE(fp)  \
       (AMC_CMD_SIZE(fp) - sizeof(struct IE_NAN_AMC_CMD))

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

uint32_t nanProcessAMC(struct ADAPTER *prAdapter, const uint8_t *buf,
		       size_t u4Size);

#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_EXT_AMC_H_ */

