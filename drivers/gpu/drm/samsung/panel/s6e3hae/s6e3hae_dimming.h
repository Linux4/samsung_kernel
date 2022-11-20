/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hae/s6e3hae_dimming.h
 *
 * Header file for S6E3HAE Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAE_DIMMING_H__
#define __S6E3HAE_DIMMING_H__
#include <linux/types.h>
#include <linux/kernel.h>
#include "../dimming.h"
#include "s6e3hae.h"

#define S6E3HAE_NR_TP (14)

#define S6E3HAE_B0_NR_LUMINANCE (256)
#define S6E3HAE_B0_TARGET_LUMINANCE (500)
#define S6E3HAE_B0_NR_HBM_LUMINANCE (357)
#define S6E3HAE_B0_TARGET_HBM_LUMINANCE (1200)

#define S6E3HAE_B0_TOTAL_NR_LUMINANCE (S6E3HAE_B0_NR_LUMINANCE + S6E3HAE_B0_NR_HBM_LUMINANCE)

#ifdef CONFIG_SUPPORT_HMD
#define S6E3HAE_RAINBOW_B0_HMD_NR_LUMINANCE (256)
#define S6E3HAE_RAINBOW_B0_HMD_TARGET_LUMINANCE (105)
#endif

#ifdef CONFIG_SUPPORT_AOD_BL
#define S6E3HAE_RAINBOW_B0_AOD_NR_LUMINANCE (4)
#define S6E3HAE_RAINBOW_B0_AOD_TARGET_LUMINANCE (60)
#endif

#endif /* __S6E3HAE_DIMMING_H__ */
