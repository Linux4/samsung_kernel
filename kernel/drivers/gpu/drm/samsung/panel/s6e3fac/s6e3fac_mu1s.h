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

#ifndef __S6E3FAC_MU1S_H__
#define __S6E3FAC_MU1S_H__
#include "../panel.h"
#include "../panel_drv.h"

enum {
	S6E3FAC_MU1S_HS_CLK_1328 = 0,
	S6E3FAC_MU1S_HS_CLK_1362,
	S6E3FAC_MU1S_HS_CLK_1368,
	MAX_S6E3FAC_MU1S_HS_CLK
};

#define S6E3FAC_MU1S_BR_INDEX_PROPERTY ("s6e3fac_mu1s_br_index")
#define S6E3FAC_MU1S_HMD_BR_INDEX_PROPERTY ("s6e3fac_mu1s_hmd_br_index")
#define S6E3FAC_MU1S_LPM_BR_INDEX_PROPERTY ("s6e3fac_mu1s_lpm_br_index")
#define S6E3FAC_MU1S_HS_CLK_PROPERTY ("s6e3fac_mu1s_hs_clk")

#endif /* __S6E3FAC_MU1S_H__ */
