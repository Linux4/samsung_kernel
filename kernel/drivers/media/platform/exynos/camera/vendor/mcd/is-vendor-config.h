/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vendor functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDOR_CONFIG_H
#define IS_VENDOR_CONFIG_H

#define USE_BINARY_PADDING_DATA_ADDED	/* for DDK signature */

#if defined(USE_BINARY_PADDING_DATA_ADDED) && (defined(CONFIG_USE_SIGNED_BINARY) || defined(CONFIG_SAMSUNG_PRODUCT_SHIP))
#define TZ_CONTROLLED_MEM_ATTRIBUTE 1
#else
#define TZ_CONTROLLED_MEM_ATTRIBUTE 0
#endif

#if defined(CONFIG_CAMERA_ESX_V01)
#include "esx/is-vendor-config_esx_v01.h"
#elif defined(CONFIG_CAMERA_ESX_V02)
#include "esx/is-vendor-config_esx_v02.h"
#elif defined(CONFIG_CAMERA_ESX_V03)
#include "esx/is-vendor-config_esx_v03.h"
#elif defined(CONFIG_CAMERA_AAX_V55X)
#include "aax_v55x/is-vendor-config_aax_v55x.h"
#else
#include "aax_v55x/is-vendor-config_aax_v55x.h" /* Default */
#endif

#endif
