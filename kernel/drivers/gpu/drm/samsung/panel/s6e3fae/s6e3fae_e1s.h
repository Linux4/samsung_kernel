/*
 * linux/drivers/video/fbdev/exynos/panel/nt36672c_m33x_00/nt36672c_m33_00.h
 *
 * Header file for TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAE_E1S_H__
#define __S6E3FAE_E1S_H__
#include "../panel.h"
#include "../panel_drv.h"

enum {  
	S6E3FAE_E1S_HS_CLK_1328 = 0,
	S6E3FAE_E1S_HS_CLK_1362,
	S6E3FAE_E1S_HS_CLK_1368,
	MAX_S6E3FAE_E1S_HS_CLK
};

#define S6E3FAE_E1S_MAX_NIGHT_LEVEL		(305)
#define S6E3FAE_E1S_NUM_NIGHT_LEVEL		(S6E3FAE_E1S_MAX_NIGHT_LEVEL+1)
#define S6E3FAE_E1S_MAX_HBM_CE_LEVEL	(3)
#define S6E3FAE_E1S_NUM_HBM_CE_LEVEL	(S6E3FAE_E1S_MAX_HBM_CE_LEVEL+1)

#endif /* __S6E3FAE_E1S_H__ */
