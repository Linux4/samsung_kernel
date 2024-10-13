/*
 * linux/drivers/video/fbdev/exynos/panel/ana6710/ana6710_dimming.h
 *
 * Header file for ANA6710 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA6710_DIMMING_H__
#define __ANA6710_DIMMING_H__
#include <linux/types.h>
#include <linux/kernel.h>
#include "../dimming.h"
#include "ana6710.h"

#define ANA6710_NR_TP (13)

#define ANA6710_A54X_NR_LUMINANCE (256)
#define ANA6710_A54X_TARGET_LUMINANCE (500)
#define ANA6710_A54X_NR_HBM_LUMINANCE (255)
#define ANA6710_A54X_TARGET_HBM_LUMINANCE (1000)

#ifdef CONFIG_USDM_PANEL_AOD_BL
#define ANA6710_A54X_AOD_NR_LUMINANCE (4)
#define ANA6710_A54X_AOD_TARGET_LUMINANCE (60)
#endif

#define ANA6710_A54X_TOTAL_NR_LUMINANCE (ANA6710_A54X_NR_LUMINANCE + ANA6710_A54X_NR_HBM_LUMINANCE)

#define ANA6710_R11S_NR_LUMINANCE (256)
#define ANA6710_R11S_TARGET_LUMINANCE (500)
#define ANA6710_R11S_NR_HBM_LUMINANCE (255)
#define ANA6710_R11S_TARGET_HBM_LUMINANCE (1000)

#ifdef CONFIG_USDM_PANEL_AOD_BL
#define ANA6710_R11S_AOD_NR_LUMINANCE (4)
#define ANA6710_R11S_AOD_TARGET_LUMINANCE (60)
#endif

#define ANA6710_R11S_TOTAL_NR_LUMINANCE (ANA6710_R11S_NR_LUMINANCE + ANA6710_R11S_NR_HBM_LUMINANCE)

#endif /* __ANA6710_DIMMING_H__ */
