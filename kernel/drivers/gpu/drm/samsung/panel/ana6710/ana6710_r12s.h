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

#ifndef __ANA6710_R12S_H__
#define __ANA6710_R12S_H__
#include "../panel.h"
#include "../panel_drv.h"
#include "../panel_debug.h"

#define ANA6710_R12S_HS_CLK_PROPERTY ("ana6710_r12s_hs_clk")
#define ANA6710_R12S_OSC_CLK_PROPERTY ("ana6710_r12s_osc_clk")

enum {
	ANA6710_R12S_HS_CLK_1328 = 0,
	ANA6710_R12S_HS_CLK_1362,
	ANA6710_R12S_HS_CLK_1368,
	MAX_ANA6710_R12S_HS_CLK
};

enum {
	ANA6710_R12S_OSC_CLK_181300 = 0,
	ANA6710_R12S_OSC_CLK_178900,
	MAX_ANA6710_R12S_OSC_CLK
};

enum ana6710_r12s_function {
	MAX_ANA6710_R12S_FUNCTION,
};

extern struct pnobj_func ana6710_r12s_function_table[MAX_ANA6710_R12S_FUNCTION];

#undef DDI_PROJECT_FUNC
#define DDI_PROJECT_FUNC(_index) (ana6710_r12s_function_table[_index])

int ana6710_r12s_ddi_init(struct panel_device *panel, void *buf, u32 len);

#endif /* __ANA6710_R12S_H__ */
