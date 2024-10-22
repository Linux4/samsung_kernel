/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _NAN_EXT_CCM_H_
#define _NAN_EXT_CCM_H_

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

/* [v2.1] Table 22. CCM Service group configuration data */
struct IE_NAN_CCM_CMD {
	/* 0x34 = 52 */
	uint8_t ucNanOui; /* NAN_CCM_CMD_OUI */
	/* 0x0000 ~ 0xFFFF */
	uint16_t u2Length;
	/* 0x00 ~ 0xFF */
	uint8_t ucMajorVersion;
	/* 0x00 ~ 0xFF */
	uint8_t ucMinorVersion;
	/* 0x00 ~ 0xFF */
	uint8_t ucRequestId;
	/*
	 * BITS(0): CCM enable
	 * 0: Disable
	 * 1: Enable
	 *
	 * BITS(1, 2): Type
	 * 00: Request
	 * 01: Response
	 * 10: Allocation Request
	 * 11: Notify
	 *
	 * BITS(3, 4): Status
	 * 00: Confirm
	 * 01: Modify
	 * 10-11: Reserved
	 *
	 * BIT(5, 7): Reserved
	 */
	uint8_t ccm_enable:1,
		cmd_type:2,
		status:2,
		reserved_1:3;
	/*
	 * In unit of 512TU
	 * Default: 32 (16.77sec)
	 */
	uint8_t ucCcmInterval;
	/*
	 * In unit of ms
	 * Default: 200
	 */
	uint16_t passive_scan_duration:10,
		reserved_2:6;
	uint8_t ucReserved_3;
	/*
	 * BITS(0, 4): CCM Size
	 *
	 * BITS(5, 9): CCM Participant Number (PN)
	 *
	 * BITS(10, 15): Reserved
	 */
	uint16_t ccm_size:5,
		ccm_pn:5,
		reserved_4:6;
	uint8_t ucReserved_5;
	uint8_t ucReserved_6;
} __KAL_ATTRIB_PACKED__;

#define CCM_CMD_HDR_LEN OFFSET_OF(struct IE_NAN_CCM_CMD, ucMajorVersion)
#define CCM_CMD_LEN(fp) (((struct IE_NAN_CCM_CMD *)(fp))->u2Length)
#define CCM_CMD_SIZE(fp) (CCM_CMD_HDR_LEN + CCM_CMD_LEN(fp))
#define CCM_CMD_BODY_SIZE(fp)	\
	(CCM_CMD_SIZE(fp) - sizeof(struct IE_NAN_CCM_CMD))

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

uint32_t nanProcessCCM(struct ADAPTER *prAdapter, const uint8_t *buf,
		       size_t u4Size);

#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_EXT_CCM_H_ */

