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

#if defined(USE_BINARY_PADDING_DATA_ADDED) && (defined(CONFIG_IS_DDK_DATA_LOAD) || defined(CONFIG_SAMSUNG_PRODUCT_SHIP))
#define USE_TZ_CONTROLLED_MEM_ATTRIBUTE
#endif

#if defined(CONFIG_CAMERA_AAS_V21S)
#include "is-vendor-config_aas_v21s.h"
#elif defined(CONFIG_CAMERA_XXT_V5)
#include "is-vendor-config_xxt_v5.h"
#elif defined(CONFIG_CAMERA_MMT_V12)
#include "is-vendor-config_mmt_v12.h"
#elif defined(CONFIG_CAMERA_AAU_V12S)
#include "is-vendor-config_aau_v12s.h"
#else
#include "is-vendor-config_xxt_v5.h"
#endif

#endif
