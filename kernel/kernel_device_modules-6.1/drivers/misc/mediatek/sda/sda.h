/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Mediatek Inc.
 */

#ifndef __MTK_SDA_H__
#define __MTK_SDA_H__

enum SDA_FEATURE {
	SDA_BUS_PARITY = 0,
	SDA_ERR_FLAG = 1,
	SDA_SYSTRACKER = 2,
	SDA_DBGTOP_DRM = 3,
	NR_SDA_FEATURE
};

enum BUS_PARITY_OP {
	BP_MCU_CLR = 0,
	NR_BUS_PARITY_OP
};

enum SYSTRACKER_OP {
	TRACKER_SW_RST = 0,
	TRACKER_IRQ_SWITCH = 1,
	NR_SYSTRACKER_OP
};

enum DBGTOP_DRM_OP {
	DRM_GET_STATUS = 0,
	DRM_SET_STATUS = 1,
	NR_DBGTOP_DRM_OP
};

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};
#endif   /*__MTK_SDA_H__*/
