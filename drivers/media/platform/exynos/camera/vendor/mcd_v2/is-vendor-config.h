/*
* Samsung Exynos5 SoC series IS driver
 *
 * exynos5 is vender functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDOR_CONFIG_H
#define IS_VENDOR_CONFIG_H

#define USE_BINARY_PADDING_DATA_ADDED             /* Apply Sign DDK/RTA Binary */

#if defined(USE_BINARY_PADDING_DATA_ADDED) && (defined(CONFIG_USE_SIGNED_BINARY) || defined(CONFIG_SAMSUNG_PRODUCT_SHIP))
#define TZ_CONTROLLED_MEM_ATTRIBUTE 1
#else
#define TZ_CONTROLLED_MEM_ATTRIBUTE 0
#endif

#define USE_AF_SLEEP_MODE

#define IS_MAX_CAL_SIZE (64 * 1024)

#if defined(CONFIG_CAMERA_AAW_A54X)
#include "aaw_v54x/is-vendor-config_aaw_v54x.h"
#elif defined(CONFIG_CAMERA_MMW_M54X)
#include "mmw_v54x/is-vendor-config_mmw_v54x.h"
#elif defined(CONFIG_CAMERA_ATW_V05)
#include "atw_v05/is-vendor-config_atw_v05.h"
#elif defined(CONFIG_CAMERA_STW_V60)
#include "stw_v60/is-vendor-config_stw_v60.h"
#elif defined(CONFIG_CAMERA_STW_V50)
#include "stw_v50/is-vendor-config_stw_v50.h"
#elif defined(CONFIG_CAMERA_AAX_A35X)
#include "aax_v35x/is-vendor-config_aax_v35x.h"
#elif defined(CONFIG_CAMERA_MMX_M35X)
#include "mmx_v35x/is-vendor-config_mmx_v35x.h"
#else
#include "aaw_v54x/is-vendor-config_aaw_v54x.h"
#endif

#endif
