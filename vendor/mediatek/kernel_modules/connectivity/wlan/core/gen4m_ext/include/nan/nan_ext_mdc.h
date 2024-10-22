/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_MDC_H_
#define _NAN_EXT_MDC_H_

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

/* [v2.1] Table 25. MDC data */
struct IE_NAN_MDC_CMD {
	/* 0x38 = 56 */
	uint8_t ucNanOui;
	/* 0x00 ~ 0xFF */
	uint8_t ucLength;
	/* 0x00 ~ 0xFF */
	uint8_t ucRequestId;
	/*
	 * BITS(0, 1): Type
	 * 00: Request
	 * 01: Response
	 * 10-11: Reserved
	 *
	 * BITS(2, 3): Subtype
	 * 00: Report
	 * 01: Update
	 * 10-11: Reserved
	 *
	 * BIT(4): Updated
	 * 0: Not updated
	 * 1: Updated
	 *
	 * BIT(5, 7): Reserved
	 */
	uint8_t cmd_type:2,
		subtype:2,
		updated:1,
		reserved_1:3;
	uint8_t ucMergingCriteriaValue;
	/*
	 * BITS(0, 3): Merging Criteria Type
	 * 0000: NAN Cluster Size (NMI based)
	 * 0001: NAN Data Link Size (NDI based)
	 * 0010: QoS
	 * 0011: Priority
	 * 0100-1111: Reserved
	 *
	 * BITS(4, 15): Merging Criteria Lifetime
	 * 1 ~ 2^12-1 Seconds
	 */
	uint16_t merging_criteria_type:4,
		merging_criteria_lifetime:12;
	/*
	 * BITS(0, 1): Update Method
	 * 00: Implicit
	 * 01: Explicit
	 * 10-11: Reserved
	 *
	 * BITS(2, 3): Update Device
	 * 00: Requester
	 * 01: Independently update (Each device)
	 *
	 * BITS(4, 7): Reserved
	 */
	uint8_t update_method:2,
		update_device:2,
		reserved_2:4;
} __KAL_ATTRIB_PACKED__;

#define MDC_CMD_HDR_LEN OFFSET_OF(struct IE_NAN_MDC_CMD, ucRequestId)
#define MDC_CMD_LEN(fp) (((struct IE_NAN_MDC_CMD *)(fp))->ucLength)
#define MDC_CMD_SIZE(fp) (MDC_CMD_HDR_LEN + MDC_CMD_LEN(fp))
#define MDC_CMD_BODY_SIZE(fp)	\
	(MDC_CMD_SIZE(fp) - sizeof(struct IE_NAN_MDC_CMD))

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

uint32_t nanProcessMDC(struct ADAPTER *prAdapter, const uint8_t *buf,
		       size_t u4Size);

#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_EXT_MDC_H_ */

