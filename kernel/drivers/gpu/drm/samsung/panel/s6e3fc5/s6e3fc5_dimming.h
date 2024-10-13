/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc5/s6e3fc5_dimming.h
 *
 * Header file for S6E3FC5 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC5_DIMMING_H__
#define __S6E3FC5_DIMMING_H__
#include <linux/types.h>
#include <linux/kernel.h>
#include "../dimming.h"
#include "s6e3fc5.h"

#define S6E3FC5_NR_TP (13)

#define S6E3FC5_A54X_NR_LUMINANCE (256)
#define S6E3FC5_A54X_TARGET_LUMINANCE (500)
#define S6E3FC5_A54X_NR_HBM_LUMINANCE (255)
#define S6E3FC5_A54X_TARGET_HBM_LUMINANCE (1000)

#ifdef CONFIG_USDM_PANEL_AOD_BL
#define S6E3FC5_A54X_AOD_NR_LUMINANCE (4)
#define S6E3FC5_A54X_AOD_TARGET_LUMINANCE (60)
#endif

#define S6E3FC5_A54X_TOTAL_NR_LUMINANCE (S6E3FC5_A54X_NR_LUMINANCE + S6E3FC5_A54X_NR_HBM_LUMINANCE)

#define S6E3FC5_R11S_NR_LUMINANCE (256)
#define S6E3FC5_R11S_TARGET_LUMINANCE (500)
#define S6E3FC5_R11S_NR_HBM_LUMINANCE (255)
#define S6E3FC5_R11S_TARGET_HBM_LUMINANCE (1000)

#ifdef CONFIG_USDM_PANEL_AOD_BL
#define S6E3FC5_R11S_AOD_NR_LUMINANCE (4)
#define S6E3FC5_R11S_AOD_TARGET_LUMINANCE (60)
#endif

#define S6E3FC5_R11S_TOTAL_NR_LUMINANCE (S6E3FC5_R11S_NR_LUMINANCE + S6E3FC5_R11S_NR_HBM_LUMINANCE)

#endif /* __S6E3FC5_DIMMING_H__ */
