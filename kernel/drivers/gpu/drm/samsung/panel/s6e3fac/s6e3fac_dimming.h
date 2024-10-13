/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fac/s6e3fac_dimming.h
 *
 * Header file for S6E3FAC Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAC_DIMMING_H__
#define __S6E3FAC_DIMMING_H__
#include <linux/types.h>
#include <linux/kernel.h>
#include "../dimming.h"
#include "s6e3fac.h"

#define S6E3FAC_NR_TP (13)

#define S6E3FAC_R0_HMD_NR_LUMINANCE (256)
#define S6E3FAC_R0_HMD_TARGET_LUMINANCE (105)
#define S6E3FAC_G0_HMD_NR_LUMINANCE (256)
#define S6E3FAC_G0_HMD_TARGET_LUMINANCE (105)
#define S6E3FAC_MU1S_HMD_NR_LUMINANCE (256)
#define S6E3FAC_MU1S_HMD_TARGET_LUMINANCE (105)

#define S6E3FAC_AOD_NR_LUMINANCE (4)
#define S6E3FAC_AOD_TARGET_LUMINANCE (60)

#define S6E3FAC_G0_NR_LUMINANCE (256)
#define S6E3FAC_G0_TARGET_LUMINANCE (500)
#define S6E3FAC_G0_NR_HBM_LUMINANCE (357)
#define S6E3FAC_G0_TARGET_HBM_LUMINANCE (1200)

#define S6E3FAC_G0_TOTAL_NR_LUMINANCE (S6E3FAC_G0_NR_LUMINANCE + S6E3FAC_G0_NR_HBM_LUMINANCE)

#define S6E3FAC_G0_NR_STEP (256)
#define S6E3FAC_G0_HBM_STEP (357)
#define S6E3FAC_G0_TOTAL_STEP (S6E3FAC_G0_NR_STEP + S6E3FAC_G0_HBM_STEP)

#define S6E3FAC_R0_NR_LUMINANCE (256)
#define S6E3FAC_R0_TARGET_LUMINANCE (500)
#define S6E3FAC_R0_NR_HBM_LUMINANCE (255)
#define S6E3FAC_R0_TARGET_HBM_LUMINANCE (1000)

#define S6E3FAC_R0_TOTAL_NR_LUMINANCE (S6E3FAC_R0_NR_LUMINANCE + S6E3FAC_R0_NR_HBM_LUMINANCE)

#define S6E3FAC_R0_NR_STEP (256)
#define S6E3FAC_R0_HBM_STEP (255)
#define S6E3FAC_R0_TOTAL_STEP (S6E3FAC_R0_NR_STEP + S6E3FAC_R0_HBM_STEP)

#define S6E3FAC_MU1S_NR_LUMINANCE (256)
#define S6E3FAC_MU1S_TARGET_LUMINANCE (500)
#define S6E3FAC_MU1S_NR_HBM_LUMINANCE (1020)
#define S6E3FAC_MU1S_TARGET_HBM_LUMINANCE (1200)

#define S6E3FAC_MU1S_TOTAL_NR_LUMINANCE (S6E3FAC_MU1S_NR_LUMINANCE + S6E3FAC_MU1S_NR_HBM_LUMINANCE)

#define S6E3FAC_MU1S_NR_STEP (256)
#define S6E3FAC_MU1S_HBM_STEP (1020)
#define S6E3FAC_MU1S_TOTAL_STEP (S6E3FAC_MU1S_NR_STEP + S6E3FAC_MU1S_HBM_STEP)

static struct tp s6e3fac_tp[S6E3FAC_NR_TP] = {
	{ .level = 0, .volt_src = VREG_OUT, .name = "VT", .bits = 12 },
	{ .level = 0, .volt_src = V0_OUT, .name = "V0", .bits = 12 },
	{ .level = 1, .volt_src = V0_OUT, .name = "V1", .bits = 11 },
	{ .level = 3, .volt_src = V0_OUT, .name = "V3", .bits = 11 },
	{ .level = 7, .volt_src = V0_OUT, .name = "V7", .bits = 11 },
	{ .level = 11, .volt_src = VT_OUT, .name = "V11", .bits = 11 },
	{ .level = 23, .volt_src = VT_OUT, .name = "V23", .bits = 11 },
	{ .level = 35, .volt_src = VT_OUT, .name = "V35", .bits = 11 },
	{ .level = 51, .volt_src = VT_OUT, .name = "V51", .bits = 11 },
	{ .level = 87, .volt_src = VT_OUT, .name = "V87", .bits = 11 },
	{ .level = 151, .volt_src = VT_OUT, .name = "V151", .bits = 11 },
	{ .level = 203, .volt_src = VT_OUT, .name = "V203", .bits = 11 },
	{ .level = 255, .volt_src = VREG_OUT, .name = "V255", .bits = 11 },
};

#endif /* __S6E3FAC_DIMMING_H__ */
