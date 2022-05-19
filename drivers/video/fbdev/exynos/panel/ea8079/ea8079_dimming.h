/*
 * linux/drivers/video/fbdev/exynos/panel/ea8079/ea8079_dimming.h
 *
 * Header file for EA8079 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8079_DIMMING_H__
#define __EA8079_DIMMING_H__
#include <linux/types.h>
#include <linux/kernel.h>
#include "../dimming.h"
#include "ea8079.h"

#define EA8079_NR_TP (11)

#define EA8079_R8S_NR_LUMINANCE (256)
#define EA8079_R8S_TARGET_LUMINANCE (420)
#define EA8079_R8S_NR_HBM_LUMINANCE (231)
#define EA8079_R8S_TARGET_HBM_LUMINANCE (800)

#define EA8079_R8S_VRR_GAMMA_INDEX_0_BR (0)
#define EA8079_R8S_VRR_GAMMA_INDEX_1_BR (64)
#define EA8079_R8S_VRR_GAMMA_INDEX_2_BR (69)
#define EA8079_R8S_VRR_GAMMA_INDEX_3_BR (185)
#define EA8079_R8S_VRR_GAMMA_INDEX_MAX_BR (486)

#ifdef CONFIG_SUPPORT_AOD_BL
#define EA8079_R8S_AOD_NR_LUMINANCE (4)
#define EA8079_R8S_AOD_TARGET_LUMINANCE (60)
#endif

#define EA8079_R8S_TOTAL_NR_LUMINANCE (EA8079_R8S_NR_LUMINANCE + EA8079_R8S_NR_HBM_LUMINANCE)
#endif /* __EA8079_DIMMING_H__ */
