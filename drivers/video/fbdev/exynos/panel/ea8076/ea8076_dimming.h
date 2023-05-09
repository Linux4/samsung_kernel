/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076_dimming.h
 *
 * Header file for EA8076 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8076_DIMMING_H__
#define __EA8076_DIMMING_H__
#include <linux/types.h>
#include <linux/kernel.h>
#include "../dimming.h"
#include "ea8076.h"

#define EA8076_NR_TP (12)

#define EA8076_R8S_NR_LUMINANCE (256)
#define EA8076_R8S_TARGET_LUMINANCE (420)
#define EA8076_R8S_NR_HBM_LUMINANCE (110)
#define EA8076_R8S_TARGET_HBM_LUMINANCE (600)

#ifdef CONFIG_SUPPORT_AOD_BL
#define EA8076_R8S_AOD_NR_LUMINANCE (4)
#define EA8076_R8S_AOD_TARGET_LUMINANCE (60)
#endif

#define EA8076_R8S_TOTAL_NR_LUMINANCE (EA8076_R8S_NR_LUMINANCE + EA8076_R8S_NR_HBM_LUMINANCE)
#endif /* __EA8076_DIMMING_H__ */
