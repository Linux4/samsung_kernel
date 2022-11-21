/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hac/s6e3hac_dimming.h
 *
 * Header file for S6E3HAC Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAC_DIMMING_H__
#define __S6E3HAC_DIMMING_H__
#include <linux/types.h>
#include <linux/kernel.h>
#include "../dimming.h"
#include "s6e3hac.h"

#define S6E3HAC_NR_TP (12)

#define S6E3HAC_CANVAS2_NR_LUMINANCE (256)
#define S6E3HAC_CANVAS2_TARGET_LUMINANCE (500)

#define S6E3HAC_CANVAS2_NR_HBM_LUMINANCE (255)
#define S6E3HAC_CANVAS2_TARGET_HBM_LUMINANCE (1000)

#ifdef CONFIG_SUPPORT_AOD_BL
#define S6E3HAC_CANVAS2_AOD_NR_LUMINANCE (4)
#define S6E3HAC_CANVAS2_AOD_TARGET_LUMINANCE (60)
#endif

#define S6E3HAC_CANVAS2_TOTAL_NR_LUMINANCE (S6E3HAC_CANVAS2_NR_LUMINANCE + S6E3HAC_CANVAS2_NR_HBM_LUMINANCE)

static struct tp s6e3hac_canvas_tp[S6E3HAC_NR_TP] = {
	{ .level = 0, .volt_src = VREG_OUT, .name = "VT", .bits = 4 },
	{ .level = 0, .volt_src = V0_OUT, .name = "V0", .bits = 4 },
	{ .level = 1, .volt_src = V0_OUT, .name = "V1", .bits = 8 },
	{ .level = 7, .volt_src = V0_OUT, .name = "V7", .bits = 8 },
	{ .level = 11, .volt_src = VT_OUT, .name = "V11", .bits = 8 },
	{ .level = 23, .volt_src = VT_OUT, .name = "V23", .bits = 8 },
	{ .level = 35, .volt_src = VT_OUT, .name = "V35", .bits = 8 },
	{ .level = 51, .volt_src = VT_OUT, .name = "V51", .bits = 8 },
	{ .level = 87, .volt_src = VT_OUT, .name = "V87", .bits = 8 },
	{ .level = 151, .volt_src = VT_OUT, .name = "V151", .bits = 8 },
	{ .level = 203, .volt_src = VT_OUT, .name = "V203", .bits = 8 },
	{ .level = 255, .volt_src = VREG_OUT, .name = "V255", .bits = 10 },
};

#endif /* __S6E3HAC_DIMMING_H__ */
