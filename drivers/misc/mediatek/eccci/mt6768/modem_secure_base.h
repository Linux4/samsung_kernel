/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __MODEM_SECURE_BASE_H__
#define __MODEM_SECURE_BASE_H__

#include <mt-plat/mtk_secure_api.h>

enum CCCI_SECURE_REQ_ID {
	MD_DBGSYS_REG_DUMP = 0,
	MD_BANK0_HW_REMAP,
	MD_BANK1_HW_REMAP,
	MD_BANK4_HW_REMAP,
	MD_SIB_HW_REMAP,
	MD_CLOCK_REQUEST,
	MD_POWER_CONFIG,
	MD_FLIGHT_MODE_SET,
	MD_HW_REMAP_LOCKED,
	MD_SEC_SMEM_INF,
	UPDATE_MD_SEC_SMEM,
};

#endif				/* __MODEM_SECURE_BASE_H__ */
